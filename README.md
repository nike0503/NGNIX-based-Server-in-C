# NGNIX-based-Web-Server-in-C

This is a simple nginx type server in C language, which hosts a simple sign-in page, and also implements additional features like reverse-proxy and multi-threading.

To run this server, first clone this repo using

``` git clone https://github.com/nike0503/NGNIX-based-Server-in-C.git```

Then compile the C-program, 

``` gcc final_srver.c -pthread```

Now to run the server with the sign-in page hosted (i.e. without proxy), run

``` ./a.out config_file_nonproxy.txt```

Open your web browser(preferably Firefox) and go to localhost:9000 to view the webpage.(port written in config file)
\
\
\
To implement the reverse-proxy feature, first write the address of the server to which it serves as proxy in the config_file_proxy.txt.
Next, run

``` ./a.out config_file_proxy.txt```

Here also, open the browser and go to localhost:7000 to view the requested webpage from the main server. (port written in config file)
