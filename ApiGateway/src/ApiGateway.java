import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.PrintWriter;
import java.io.StringReader;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.Executors;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.locks.ReentrantLock;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.Socket;

import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;

import javax.json.Json;
import javax.json.JsonObject;
import javax.json.JsonReader;

import com.sun.net.httpserver.HttpServer;
import com.sun.net.httpserver.HttpHandler;
import com.sun.net.httpserver.Headers;
import com.sun.net.httpserver.HttpExchange;

import com.rabbitmq.client.Channel;
import com.rabbitmq.client.Connection;
import com.rabbitmq.client.ConnectionFactory;
import com.rabbitmq.client.DeliverCallback;

/**
 * <b>ApiGateway App</b><br>
 * This is the only Class necessary for the microservice.
 */

public class ApiGateway {

    public final static boolean DEBUG = false;

    private final static String QUEUE_NAME = "rabbit-msbasics";
    private static ReentrantLock sync = new ReentrantLock();

    private static HashMap<String, OutputStream> clients = new HashMap<String, OutputStream>();

    /*
     * This is for HTTPS only private static SSLContext getSSLContext() throws
     * Exception {
     * 
     * char[] passphrase = "samplepass".toCharArray();
     * 
     * // System.out.println("Loading SSL Context"); //
     * System.out.println("KeyStore Type: " + KeyStore.getDefaultType()); KeyStore
     * ks = KeyStore.getInstance(KeyStore.getDefaultType()); // Generate a file
     * named "keystore" with the offline tool "keytool.exe" ks.load(new
     * FileInputStream("keystore"), passphrase);
     * 
     * // System.out.println("KeyManagerFactory Algorithm: " +
     * KeyManagerFactory.getDefaultAlgorithm()); KeyManagerFactory kmf =
     * KeyManagerFactory.getInstance(KeyManagerFactory.getDefaultAlgorithm());
     * kmf.init(ks, passphrase);
     * 
     * // System.out.println("TrustManagerFactory Algorithm: " +
     * TrustManagerFactory.getDefaultAlgorithm()); TrustManagerFactory tmf =
     * TrustManagerFactory.getInstance(TrustManagerFactory.getDefaultAlgorithm());
     * tmf.init(ks);
     * 
     * SSLContext ssl = SSLContext.getInstance("TLS");
     * ssl.init(kmf.getKeyManagers(), tmf.getTrustManagers(), null); //
     * System.out.println("SSLContext protocol: " + ssl.getProtocol());
     * 
     * return ssl; };
     */

    /**
     * <b>ApiGateway main</b><br>
     * This is the main function of the microservice<br>
     * 
     * @param args arguments are not evaluated
     * @throws java.io.IOException if a connection problem occurs
     */

    public static void main(String[] args) throws IOException, TimeoutException {

        final ThreadPoolExecutor threadPoolExecutor = (ThreadPoolExecutor) Executors.newFixedThreadPool(10);

        HttpHandler rootHandler = new HttpHandler() {
            @Override
            public void handle(HttpExchange t) throws IOException {
                String request = t.getRequestMethod();
                if ("GET".equals(request)) {
                    handleResponse(t, t.getRequestURI().toString());
                } else {
                    if (DEBUG)
                        System.out.println("(MSBasics-ApiGateway) request " + request + " not handled here");
                    t.sendResponseHeaders(501, -1);
                }
                t.close();
            }

            private void handleResponse(HttpExchange t, String p) throws IOException {
                if (DEBUG)
                    System.out.println("(MSBasics-ApiGateway) handling '/' start");
                InputStream is = t.getRequestBody();
                is.readAllBytes();
                is.close();
                if ("/".equals(p)) {
                    Headers h = t.getResponseHeaders();
                    if (h.containsKey("Content-type"))
                        h.set("Content-type", "text/html");
                    else
                        h.add("Content-type", "text/html");
                    t.sendResponseHeaders(200, 0);
                    OutputStream os = t.getResponseBody();
                    BufferedReader reader = new BufferedReader(new FileReader("index.html"));
                    try {
                        String response;
                        while ((response = reader.readLine()) != null) {
                            os.write(response.getBytes());
                        }
                    } catch (IOException e) {
                        e.printStackTrace();
                    } finally {
                        reader.close();
                    }
                    os.flush();
                    os.close();
                } else {
                    if (DEBUG)
                        System.out.println("(MSBasics-ApiGateway) GET request: unknown parameter: " + p);
                    t.sendResponseHeaders(400, -1);
                }
                if (DEBUG)
                    System.out.println("(MSBasics-ApiGateway) handling '/' done");
            }
        };

        HttpHandler sseHandler = new HttpHandler() {
            @Override
            public void handle(HttpExchange t) throws IOException {
                if (DEBUG) System.out.println("(MSBasics-ApiGateway) handling 'sse' start");
                InputStream is = t.getRequestBody();
                is.readAllBytes();
                is.close();
                String request = t.getRequestMethod();
                String UID = t.getRequestURI().toString();
                UID = UID.substring(UID.length() - 5);
                if ("GET".equals(request)) {
                    if (t.getRequestURI().toString().startsWith("/sse/close")) {
                        clients.get(UID).close();
                        sync.lock();
                        clients.remove(UID);
                        sync.unlock();
                        if (DEBUG) System.out.println("(MSBasics-ApiGateway) removed client: UID=" + UID);
                        t.close();
                    } else {
                        Headers h = t.getResponseHeaders();
                        if (h.containsKey("Content-type"))
                            h.set("Content-type", "text/event-stream");
                        else
                            h.add("Content-type", "text/event-stream");
                        if (h.containsKey("Cache-control"))
                            h.set("Cache-control", "no-cache");
                        else
                            h.add("Cache-control", "no-cache");
                        t.sendResponseHeaders(200, 0);
                        sync.lock();
                        clients.put(UID, t.getResponseBody());
                        sync.unlock();
                        if (DEBUG) System.out.println("(MSBasics-ApiGateway) added client: UID=" + UID);
                    }
                } else {
                    t.sendResponseHeaders(200, -1);
                    t.close();
                }

                if (DEBUG) System.out.println("(MSBasics-ApiGateway) handling 'sse' done");
            }
        };

        HttpHandler iconHandler = new HttpHandler() {
            @Override
            public void handle(HttpExchange t) throws IOException {

                if (DEBUG)
                    System.out.println("(MSBasics-ApiGateway) handling 'icon' start");
                String request = t.getRequestMethod();
                if ("GET".equals(request)) {
                    byte[] bytes = Files.readAllBytes(Paths.get("favicon.ico"));
                    t.sendResponseHeaders(200, bytes.length);
                    try (OutputStream ostream = t.getResponseBody()) {
                        ostream.write(bytes);
                        ostream.close();
                    }
                } else { // any other request but GET
                    t.sendResponseHeaders(200, -1);
                }
                if (DEBUG)
                    System.out.println("(MSBasics-ApiGateway) handling 'icon' done");
                t.close();
            }
        };

        HttpHandler resetHandler = new HttpHandler() {
            private final String resetheaderstub = "PUT /api/v1/ HTTP/1.1\r\nHost: localhost:8082\r\nContent-Type: application/json\r\nAccept: */*\r\nContent-Length: ";
            private final String resetcmdjson = "{\"data\": {\"attributes\": {\"name\": \"servicejava\"}},\"meta\":{\"action\":\"reset\"}}";

            private void triggerReset() throws IOException {
                String response = "";
                String latestCounter = "null";
                String latestTimestamp = "n.a.";
                String latestErrorMsg = "n.a.";
                try {
                    PrintWriter out;
                    InputStreamReader in;
                    Socket clientSocket = new Socket("servicec", 8082);
                    out = new PrintWriter(clientSocket.getOutputStream(), true);
                    in = new InputStreamReader(clientSocket.getInputStream());
                    String resetcmd = resetheaderstub + resetcmdjson.length() + "\r\n\r\n" + resetcmdjson;
                    out.println(resetcmd);
                    out.flush();
                    byte[] bch = new byte[1024];
                    int count = 0;
                    do {
                        bch[count++] = (byte) in.read();
                    } while (in.ready() && (count < 1024));
                    in.close();
                    out.close();
                    clientSocket.close();
                    if (count >= 1024) {
                        latestCounter = "ERR";
                        latestErrorMsg = "Buffer too small";
                    } else {
                        response = new String(bch, StandardCharsets.UTF_8).trim();
                    }
                } catch (IOException e) {
                    latestErrorMsg = "Input/Output error";
                }
                if ("null".equals(latestCounter)) {
                    // there is a response starting with
                    // HTTP/1.1 200 OK - or -
                    // HTTP/1.1 4xx <error>
                    // (1) split response into lines
                    String[] resp = response.split("\r\n");
                    // (2) get HTTP response code
                    int httpCode;
                    try {
                        httpCode = Integer.parseInt(resp[0].split(" ")[1]);
                    } catch (NumberFormatException e) {
                        httpCode = -1;
                    }
                    if (httpCode == -1) {
                        latestErrorMsg = "Invalid HTTP response: " + resp[0];
                    } else if (httpCode != 200) {
                        // (3a) parse Error Message
                        int i;
                        for (i = 0; i < resp.length; i++) {
                            if (resp[i].indexOf("errors") > 0)
                                break;
                        }
                        if (i == resp.length)
                            latestErrorMsg = "HTTP Error " + httpCode + ": missing error description";
                        else {
                            JsonReader jsonReader = Json.createReader(new StringReader(resp[i]));
                            JsonObject jo = jsonReader.readObject();
                            jsonReader.close();
                            if (jo == null)
                                latestErrorMsg = "Invalid error message";
                            else {
                                JsonObject joattr = jo.getJsonObject("errors");
                                if (joattr == null)
                                    latestErrorMsg = "Invalid error message";
                                else
                                    latestErrorMsg = httpCode + " - " + joattr.getString("title", "unknown type") + ": "
                                            + joattr.getString("details", "unknown details");
                            }
                        }
                    } else {
                        latestCounter = "ERR";
                        // (3b) parse data, extract latestCounter
                        int i;
                        for (i = 0; i < resp.length; i++) {
                            if (resp[i].indexOf("servicejava") > 0)
                                break;
                        }
                        if (i == resp.length)
                            latestErrorMsg = "missing data field in response string";
                        else {
                            // (3b) parse data, extract lastCounter
                            JsonReader jsonReader = Json.createReader(new StringReader(resp[i]));
                            JsonObject jo = jsonReader.readObject();
                            jsonReader.close();
                            if (jo == null)
                                latestErrorMsg = "invalid data string";
                            else {
                                JsonObject joattr = jo.getJsonObject("data").getJsonObject("attributes");
                                if (joattr == null)
                                    latestErrorMsg = "Invalid data string";
                                else {
                                    latestTimestamp = joattr.getString("timestamp", "invalid");
                                    latestCounter = joattr.getString("counter", "ERR");
                                    if ("ERR".equals(latestCounter))
                                        latestErrorMsg = "Invalid data string";
                                    else
                                        latestErrorMsg = "none";
                                } 
                            }
                        }
                    }
                }
               try {
                    response = "event: msg-reset\ndata: { \"counter\": \"" + latestCounter 
                                                    + "\",\"timestamp\":\"" + latestTimestamp 
                                                    + "\",\"errormsg\":\"" + latestErrorMsg + "\"}\n\n";
                    sync.lock();
                    for (Map.Entry<String,OutputStream> me : clients.entrySet()) {
                        OutputStream os = me.getValue();
                        os.write(response.getBytes(StandardCharsets.UTF_8));
                        os.flush();
                    }
                }    
                finally {
                    sync.unlock();
                }
    
            }

            @Override
            public void handle(HttpExchange t) throws IOException {

                if (DEBUG)
                    System.out.println("(MSBasics-ApiGateway) handling 'reset' start");
                triggerReset();
                t.sendResponseHeaders(200, -1);
                t.getResponseBody().close();
                if (DEBUG)
                    System.out.println("(MSBasics-ApiGateway) handling 'reset' done");
            }
        };

        HttpHandler summaryHandler = new HttpHandler() {
        
            @Override
            public void handle(HttpExchange t) throws IOException {

                if (DEBUG)
                    System.out.println("(MSBasics-ApiGateway) handling 'summary' start");
                t.sendResponseHeaders(200, 0);
                JsonObject jo = null;
                try {
                    InputStream is = t.getRequestBody();
                    JsonReader jsonReader = Json.createReader(is);
                    jo = jsonReader.readObject();
                    jsonReader.close();
                    is.close();
                } catch (Exception e) {
                    e.printStackTrace();
                }
                t.getResponseBody().close();
                if (DEBUG) System.out.println("(MSBasics-ServiceJava) received: " + jo.toString());                    
                JsonObject jTmp = jo.getJsonObject("data");
                if (jTmp != null) {
                    String response;
                    try {
                        response = "event: msg-summary\ndata: " + jTmp.getString("summary") + "\n\n"; 
                        sync.lock();
                        for (Map.Entry<String,OutputStream> me : clients.entrySet()) {
                            OutputStream os = me.getValue();
                            os.write(response.getBytes(StandardCharsets.UTF_8));
                            os.flush();
                        }
                    }    
                    finally {
                        sync.unlock();
                    }
                }
                if (DEBUG)
                    System.out.println("(MSBasics-ApiGateway) handling 'summary' done");
            }
        };


        /*
        // This is for HTTPS, instead of HTTP
        final HttpsServer server = HttpsServer.create(new InetSocketAddress(InetAddress.getByName("0.0.0.0"), 8080), 0); 
        try { 
            SSLContext sslContext = getSSLContext();
            server.setHttpsConfigurator (new HttpsConfigurator(sslContext) { 
                public void configure (HttpsParameters params) { 
                    InetSocketAddress remote = params.getClientAddress(); 
                    SSLContext c = getSSLContext(); 
                    SSLParameters sslparams = c.getDefaultSSLParameters(); 
                    params.setSSLParameters(sslparams);
                }
            });
        } catch (Exception e) { 
            System.out.println("Error creating SSLContext: " + e.getMessage()); System.exit(-1); 
        }
        */

        final HttpServer server = HttpServer.create(new InetSocketAddress(InetAddress.getByName("0.0.0.0"), 8080), 0);
        server.createContext("/", rootHandler);
        server.createContext("/sse", sseHandler);
        server.createContext("/favicon.ico", iconHandler);
        server.createContext("/reset", resetHandler);
        server.createContext("/summary", summaryHandler);


        server.setExecutor(threadPoolExecutor);

        Runtime.getRuntime().addShutdownHook(new Thread(new Runnable() {
            @Override
            public void run() {
                System.out.println("(MSBasics-ApiGateway) service terminated");
                server.stop(0);
            }
        }));

        server.start();

        ConnectionFactory factory = new ConnectionFactory();
        factory.setHost("rabbit-msbasics");
        Connection connection = null;
        Channel channel = null;
        try {
            connection = factory.newConnection();
            channel = connection.createChannel();
            Map<String, Object> qargs = new HashMap<String, Object>();
            qargs.put("x-message-ttl", 60000);
            channel.queueDeclare(QUEUE_NAME, false, false, false, qargs);
        } catch (IOException e) {
            if ((channel != null) && (channel.isOpen()))
                channel.close();
            if ((connection != null) && (connection.isOpen()))
                connection.close();
            e.printStackTrace();
        }

        DeliverCallback deliverCallback = (consumerTag, delivery) -> {
            String str = new String(delivery.getBody(), StandardCharsets.UTF_8);
            String response = "event: msg-count\ndata: " + str + "\n\n";
            try {
                sync.lock();
                for (Map.Entry<String,OutputStream> me : clients.entrySet()) {
                    OutputStream os = me.getValue();
                    os.write(response.getBytes(StandardCharsets.UTF_8));
                    os.flush();
                }
            }
            finally {
                sync.unlock();
            }
        };

        channel.basicConsume(QUEUE_NAME, true, deliverCallback, consumerTag -> { });
        System.out.println("(MSBasics-ApiGateway) service started");
    }
}
