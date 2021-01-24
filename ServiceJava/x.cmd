docker kill servicejava
docker rm servicejava
docker image rm servicejava
docker build -t servicejava .
docker run -dit --name servicejava --network msbasics-network -p 8081:8081 servicejava