#!/usr/bin/env node
import * as cdk from 'aws-cdk-lib';
import { BrickBlasterSiteStack } from '../lib/site-stack';

// The PERSONAL AWS account, pinned (same guard as xout/infra): with credentials of the
// bizmobile work account active, CDK refuses instead of deploying there.
const COMPTE_PERSO = '103388474662';
const compteEnv = process.env.CDK_DEFAULT_ACCOUNT;
if (compteEnv && compteEnv !== COMPTE_PERSO) {
    throw new Error(
        `Refus : cette stack appartient au compte perso ${COMPTE_PERSO}, l'environnement designe ${compteEnv}. ` +
        `Deploie via : bash /mnt/d/tools/aws-perso.sh --zone takohi.me -- node node_modules/aws-cdk/bin/cdk deploy`
    );
}

const app = new cdk.App();
new BrickBlasterSiteStack(app, 'BrickBlasterSiteStack', {
    env: { account: COMPTE_PERSO, region: 'us-east-1' },
});
