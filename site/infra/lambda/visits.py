"""Visitor counter of brickblaster.takohi.me: GET reads it, POST adds one.
Reached through CloudFront at /api/visits (Lambda function URL origin)."""
import json
import os

import boto3

TABLE = boto3.resource('dynamodb').Table(os.environ['TABLE_NAME'])
KEY = {'pk': 'VISITS'}


def reply(status, body):
    return {'statusCode': status,
            'headers': {'content-type': 'application/json', 'cache-control': 'no-store'},
            'body': json.dumps(body)}


def handler(event, context):
    method = event.get('requestContext', {}).get('http', {}).get('method', '')
    if event.get('rawPath') != '/api/visits' or method not in ('GET', 'POST'):
        return reply(404, {'error': 'not found'})
    if method == 'POST':
        item = TABLE.update_item(Key=KEY, UpdateExpression='ADD visits :one',
                                 ExpressionAttributeValues={':one': 1},
                                 ReturnValues='UPDATED_NEW')['Attributes']
    else:
        item = TABLE.get_item(Key=KEY, ConsistentRead=True).get('Item', {})
    return reply(200, {'visits': int(item.get('visits', 0))})
