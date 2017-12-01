set -e
mkdir -p build
cd build
cmake ../
make
for t in tests/test*; do $t; done
cd ../

echo 'Deploying...'
scp -i ~/.blippar-ssh/id_rsa scripts/times.py michael@10.11.12.191:/mnt/michal/ngine/
scp -i ~/.blippar-ssh/id_rsa build/server michael@10.11.12.191:/mnt/michal/ngine/
