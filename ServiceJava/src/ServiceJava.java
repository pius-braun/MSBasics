import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.nio.charset.StandardCharsets;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.TimeoutException;

import javax.json.Json;
import javax.json.JsonObject;
import javax.json.JsonReader;

import com.rabbitmq.client.Channel;
import com.rabbitmq.client.Connection;
import com.rabbitmq.client.ConnectionFactory;
import com.sun.net.httpserver.Headers;
import com.sun.net.httpserver.HttpExchange;
import com.sun.net.httpserver.HttpHandler;
import com.sun.net.httpserver.HttpServer;

/** 
  * <b>ServiceJava App</b><br>
  * This is the only Class necessary for the microservice.<br>
  * It's "main" method provides the functionality
 */

public class ServiceJava {

    public final static boolean DEBUG = false;
    
    private final static String QUEUE_NAME = "rabbit-msbasics";
    private static int counter = 0;


/** 
  * <b>ServiceJava main method</b><br>
  * This is the main function of the  microservice<br>
  * It implements:
  * (1) a counter, increased every second, and sent to message queue (rabbitmq)
  * (2) a REST server to receive and parse POST with the json document: {"meta":{"action":"reset"}}
  * (3) a response is generated on the "reset": {"data": {"type": "counter", "value": <latest counter value>}}
  * (4) some errors (not POST, malformed json-document) are responded by error 400.
  * @param args arguments are not evaluated
  * @throws java.lang.Exception if a connection problem occurs 
 */ 

     public static void main(String[] args) throws IOException,TimeoutException {

        HttpHandler restHandler = new HttpHandler() {
            @Override
            public void handle(HttpExchange t) throws IOException {
                String request = t.getRequestMethod();
                Headers header = t.getResponseHeaders();
                if ("POST".equals(request)) {

                    if (DEBUG) System.out.println("(MSBasics-ServiceJava) Processing POST");
                    
                    InputStream is = t.getRequestBody();
                    JsonReader jsonReader = Json.createReader(is);
                    JsonObject jo = jsonReader.readObject();
                    jsonReader.close();
                    is.close();
                    
                    if (DEBUG) System.out.println("(MSBasics-ServiceJava) received: " + jo.toString());                    
                    JsonObject jTmp = jo.getJsonObject("meta");
                    if (jTmp == null) {

                        t.sendResponseHeaders(400, -1);
                        t.getResponseBody().close();

                        if (DEBUG) System.out.println("(MSBasics-ServiceJava) invalid json format");
                    }
                    else {
                        if (DEBUG) System.out.println("(MSBasics-ServiceJava) action = " + jTmp.getString("action"));
                        
                        if ("reset".equals(jo.getJsonObject("meta").getString("action"))) {
                            JsonObject jresponse = (JsonObject) Json.createObjectBuilder()
                                .add("data", Json.createObjectBuilder()
                                .add("type", "counter")
                                .add("value", counter)).build();
                            String response = jresponse.toString();
                            
                            if (DEBUG) System.out.println("(MSBasics-ServiceJava) Sending: >" + response + "< Length: " + response.length());
                            
                            header.add("Content-Type", "application/json");
                            header.add("Transfer-Encoding", "chunked");
                            header.add("Content-Length", Integer.toString(response.length()));
                            t.sendResponseHeaders(200, 0);
                            
                            if (DEBUG) System.out.println("(MSBasics-ServiceJava) response header sent");
                            
                            OutputStream ostream = t.getResponseBody();
                            try {
                                ostream.write(response.getBytes(StandardCharsets.UTF_8));
                                ostream.flush();
                            } catch (IOException e)  {
                                if (DEBUG) System.out.println("(MSBasics-ServiceJava) IOException");
                                ostream.close();
                                e.printStackTrace();
                            }
                            counter = 0;
                            
                            if (DEBUG) System.out.println("(MSBasics-ServiceJava) response body sent");
                            
                            ostream.close();
                        } else {
                            t.sendResponseHeaders(400, -1);
                            t.getResponseBody().close();
                            if (DEBUG) System.out.println("(MSBasics-ServiceJava) invalid json format");
                        }
                    }
                } else { // any other request but POST
                    InputStream is = t.getRequestBody();
                    is.readAllBytes();
                    is.close();
                    t.sendResponseHeaders(400, -1);
                    t.getResponseBody().close();
                    if (DEBUG) System.out.println("(MSBasics-ServiceJava) POST request expected");
                }
            }
        };

        final HttpServer server = HttpServer.create(new InetSocketAddress(InetAddress.getByName("0.0.0.0"), 8081), 0);
        server.createContext("/api/v1/", restHandler);

        server.start();
    
        ConnectionFactory factory = new ConnectionFactory();
        // for test on host
        // factory.setHost("0.0.0.0");
        // for docker container on same network: the host is the name of rabbitmq container
        factory.setHost("rabbit-msbasics");
        counter = 1;
        Connection connection = null;
        Channel channel = null;
        try {
            connection = factory.newConnection();
            channel = connection.createChannel();
            Map<String, Object> qargs = new HashMap<String, Object>();  
            qargs.put("x-message-ttl", 60000);
            channel.queueDeclare(QUEUE_NAME, false, false, false, qargs);
            System.out.println("(MSBasics-ServiceJava) Service started");
            for (;; counter++) {
                Thread.sleep(1000);
                channel.basicPublish("", QUEUE_NAME, null, Integer.toString(counter).getBytes());
            }
        } catch (IOException e) {
            if (channel != null) channel.close();
            if (connection != null) connection.close();
            server.stop(0);
            e.printStackTrace();
        }
        catch (InterruptedException e) {
            channel.close();
            connection.close();
            server.stop(0);
            System.out.println("(MSBasics-ServiceJava) Service terminated");
        }
    
    }
}
