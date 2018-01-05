echo 'Deploying...'
scp -i ~/.blippar-ssh/id_rsa scripts/results.py michael@10.11.12.191:/mnt/michal/ngine/
scp -i ~/.blippar-ssh/id_rsa scripts/count_nonessential.py michael@10.11.12.191:/mnt/michal/ngine/
scp -i ~/.blippar-ssh/id_rsa scripts/ta.py michael@10.11.12.191:/mnt/michal/ngine/
scp -i ~/.blippar-ssh/id_rsa scripts/wand.py michael@10.11.12.191:/mnt/michal/ngine/
scp -i ~/.blippar-ssh/id_rsa scripts/dochits.py michael@10.11.12.191:/mnt/michal/ngine/
scp -i ~/.blippar-ssh/id_rsa build/server michael@10.11.12.191:/mnt/michal/ngine/
scp -i ~/.blippar-ssh/id_rsa build/index_stats michael@10.11.12.191:/mnt/michal/ngine/
scp -i ~/.blippar-ssh/id_rsa build/maxscores michael@10.11.12.191:/mnt/michal/ngine/
scp -i ~/.blippar-ssh/id_rsa build/acchits michael@10.11.12.191:/mnt/michal/ngine/
scp -i ~/.blippar-ssh/id_rsa build/runtaat michael@10.11.12.191:/mnt/michal/ngine/
scp -i ~/.blippar-ssh/id_rsa build/rundaat michael@10.11.12.191:/mnt/michal/ngine/
