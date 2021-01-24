@echo off
rem Clear errorlevel
set errorlevel=
type nul>nul
docker info >nul
if errorlevel 1 (
  echo Please start Docker Desktop first.
  echo command file exits. 
  goto endcmd
)
echo starting services
docker network inspect msbasics-network >nul
if errorlevel 1 (
  echo .. starting bridge network
  docker network create msbasics-network
)
docker container inspect rabbit-msbasics >nul
if errorlevel 1 (
  echo .. starting message queue service 
  docker run -d --hostname msbasics-host --network msbasics-network --name rabbit-msbasics rabbitmq
  echo .. please wait 20 seconds for rabbit-masbasics to start
)
docker container start rabbit-msbasics
echo .. done
:endcmd
