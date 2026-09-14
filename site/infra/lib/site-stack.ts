import * as cdk from 'aws-cdk-lib';
import * as s3 from 'aws-cdk-lib/aws-s3';
import * as cloudfront from 'aws-cdk-lib/aws-cloudfront';
import * as origins from 'aws-cdk-lib/aws-cloudfront-origins';
import * as acm from 'aws-cdk-lib/aws-certificatemanager';
import * as route53 from 'aws-cdk-lib/aws-route53';
import * as targets from 'aws-cdk-lib/aws-route53-targets';
import * as s3deploy from 'aws-cdk-lib/aws-s3-deployment';
import * as dynamodb from 'aws-cdk-lib/aws-dynamodb';
import * as lambda from 'aws-cdk-lib/aws-lambda';
import { Construct } from 'constructs';
import * as path from 'path';

// brickblaster.takohi.me: the static page (site/) and its visitor counter (/api/visits).
// Same shape as xout.takohi.me (xout/infra), plus the counter behind the same CloudFront
// distribution, so the page calls it on its own origin, without CORS.
export class BrickBlasterSiteStack extends cdk.Stack {
    constructor(scope: Construct, id: string, props?: cdk.StackProps) {
        super(scope, id, props);

        const domainName = 'brickblaster.takohi.me';

        const bucket = new s3.Bucket(this, 'WebBucket', {
            removalPolicy: cdk.RemovalPolicy.DESTROY,
            autoDeleteObjects: true,
            blockPublicAccess: s3.BlockPublicAccess.BLOCK_ALL,
        });

        const zone = route53.HostedZone.fromLookup(this, 'Zone', { domainName: 'takohi.me' });
        const certificate = new acm.Certificate(this, 'Certificate', {
            domainName,
            validation: acm.CertificateValidation.fromDns(zone),
        });

        // The count lives in one item; RETAIN keeps it if the stack is ever rebuilt.
        const table = new dynamodb.Table(this, 'Visits', {
            partitionKey: { name: 'pk', type: dynamodb.AttributeType.STRING },
            billingMode: dynamodb.BillingMode.PAY_PER_REQUEST,
            removalPolicy: cdk.RemovalPolicy.RETAIN,
        });
        const counter = new lambda.Function(this, 'VisitsFunction', {
            runtime: lambda.Runtime.PYTHON_3_12,
            handler: 'visits.handler',
            code: lambda.Code.fromAsset(path.join(__dirname, '../lambda')),
            environment: { TABLE_NAME: table.tableName },
            timeout: cdk.Duration.seconds(5),
            memorySize: 128,
        });
        table.grantReadWriteData(counter);
        const counterUrl = counter.addFunctionUrl({ authType: lambda.FunctionUrlAuthType.NONE });

        const distribution = new cloudfront.Distribution(this, 'CDN', {
            defaultBehavior: {
                origin: origins.S3BucketOrigin.withOriginAccessControl(bucket),
                viewerProtocolPolicy: cloudfront.ViewerProtocolPolicy.REDIRECT_TO_HTTPS,
                cachePolicy: cloudfront.CachePolicy.CACHING_OPTIMIZED,
                compress: true,
            },
            additionalBehaviors: {
                // Never cached, POST allowed; the function URL wants its own Host header.
                '/api/*': {
                    origin: new origins.FunctionUrlOrigin(counterUrl),
                    viewerProtocolPolicy: cloudfront.ViewerProtocolPolicy.HTTPS_ONLY,
                    allowedMethods: cloudfront.AllowedMethods.ALLOW_ALL,
                    cachePolicy: cloudfront.CachePolicy.CACHING_DISABLED,
                    originRequestPolicy: cloudfront.OriginRequestPolicy.ALL_VIEWER_EXCEPT_HOST_HEADER,
                },
            },
            domainNames: [domainName],
            certificate,
            defaultRootObject: 'index.html',
            priceClass: cloudfront.PriceClass.PRICE_CLASS_100,
            minimumProtocolVersion: cloudfront.SecurityPolicyProtocol.TLS_V1_2_2021,
        });

        const alias = route53.RecordTarget.fromAlias(new targets.CloudFrontTarget(distribution));
        new route53.ARecord(this, 'SiteA', { zone, recordName: 'brickblaster', target: alias });
        new route53.AaaaRecord(this, 'SiteAAAA', { zone, recordName: 'brickblaster', target: alias });

        new s3deploy.BucketDeployment(this, 'Deploy', {
            sources: [s3deploy.Source.asset(path.join(__dirname, '../..'), {
                exclude: ['infra', 'infra/**', '*.md'],
            })],
            destinationBucket: bucket,
            distribution,
            distributionPaths: ['/*'],
        });

        new cdk.CfnOutput(this, 'SiteUrl', { value: `https://${domainName}` });
        new cdk.CfnOutput(this, 'DistributionId', { value: distribution.distributionId });
        new cdk.CfnOutput(this, 'BucketName', { value: bucket.bucketName });
    }
}
