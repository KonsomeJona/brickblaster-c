# brickblaster.takohi.me

The page of the game (`index.html`, `style.css`, `compteur.js`, `img/`) and its visitor
counter. Colours from the 1999 Média Pocket box: black space, red-orange frame, red band
with yellow letters; the counter uses the pink of the in-game score.

`infra/` deploys it with AWS CDK to the personal account: S3 + CloudFront +
certificate + `brickblaster` records in the `takohi.me` zone, and `/api/visits` (Lambda
function URL + one DynamoDB item) behind the same distribution.

The counter counts one visit per browser: the first visit sends POST, later ones GET.
It is not protected against someone calling POST in a loop.

Deploy. `node_modules` is a symlink, never a folder on E:. `npm install` on D: takes
tens of minutes (thousands of small files over DrvFs), so the 2026-09-14 deploy linked
the already installed CDK of X-Out (same versions). `cdk.out` goes to `/tmp` (see
`cdk.json`): on D:, CDK's asset copy fails with `EPERM copyfile`.

```bash
ln -s /mnt/e/dev/mono-google-play-apps/xout/infra/node_modules infra/node_modules
cd infra && bash /mnt/d/tools/aws-perso.sh --zone takohi.me -- node node_modules/aws-cdk/bin/cdk deploy
```
