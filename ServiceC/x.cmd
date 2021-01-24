docker kill servicec
docker rm servicec
docker image rm servicec
docker build -t servicec .
docker run -dit --name servicec --network msbasics-network -p 8082:8082 servicec
