docker kill servicepython
docker rm servicepython
docker image rm servicepython
docker build -t servicepython .
docker run -dit --name servicepython --network msbasics-network -p 8083:8083 servicepython