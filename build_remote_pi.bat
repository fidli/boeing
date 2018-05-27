set TARGET_ADDR=10.0.0.61
set TARGET_DIR=boeing
set TARGET=%TARGET_ADDR%:%TARGET_DIR%

call scp -i ssh/id_rsa build/data/boeing1.config pi@%TARGET%/build/data/boeing.config
call rsync -c -e "ssh -i ssh/id_rsa" -r sources pi@%TARGET%
call rsync -c -L -e "ssh -i ssh/id_rsa" -r baselib pi@%TARGET%
call rsync -c -e "ssh -i ssh/id_rsa" build.sh pi@%TARGET%
call rsync -c -e "ssh -i ssh/id_rsa" Makefile pi@%TARGET%

ssh -i ssh/id_rsa pi@%TARGET_ADDR% "cd %TARGET_DIR%;./build.sh"
