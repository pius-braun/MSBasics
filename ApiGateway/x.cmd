docker kill apigateway
docker rm apigateway
docker image rm apigateway
docker build -t apigateway .
docker run -dit --name apigateway --network msbasics-network -p 8080:8080 apigateway
