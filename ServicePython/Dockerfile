FROM python:alpine
WORKDIR /usr/src/app
COPY requirements.txt ./
RUN pip install --no-cache-dir -r requirements.txt
COPY ServicePython.py .
EXPOSE 8083
CMD [ "python", "./ServicePython.py" ]
