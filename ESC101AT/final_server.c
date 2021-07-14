#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>
#include <signal.h>
#include <pthread.h>
int app_fd,sockfd,n,threadcount=0;
char addr_app_serv[50];
int port_app,app_fd;
char head_logfile[50][20]; int no_of_headers = 0;
char config_file[60],config_buf[5000],logfile[100];
void port_no(char *buf, int *portno) {
	*portno = 0;
	char *ptr = strstr(buf,"Port");
	ptr = ptr + 7;
	int i = 0;
	while(ptr[i]!= ' ') {
		int a = ptr[i] - '0';
		i++;
		(*portno) = (*portno) * 10 + a;
	}
}
void log_file_address(char *buf, char *log) {
	char *ptr = strstr(buf,"Logfile");
	ptr = ptr + 10;
	int i = 0;
	while(ptr[i] != ' ') {
		log[i] = ptr[i];
		i++;
	}
	log[i] = '\0';
}
void sort_head(char heads[][20],int count) {
	for(int i = 0; i < count - 1; i++) {
		for(int j = i+1; j < count ; j++) {
			if(strcmp(heads[i],heads[j]) > 0) {
				char temp[100];
				strcpy(temp,heads[i]);
				strcpy(heads[i],heads[j]);
				strcpy(heads[j],temp);
			}
		}
	}	
}
void get_log_head(char *buf) {
	char *ptr = strstr(buf,"Log_Headers");
	if(ptr == NULL) {
		//printf("\nNo extra log headers\n");
		return;
	}
	ptr += 15;
	int j = 0;
	while(ptr[j] != '}') {
		if(ptr[j] ==',')
			no_of_headers++;
		j++;
	}
	if(j != 0)
		no_of_headers++;
	for(int i = 0; i < no_of_headers; i++) {
		for(int j = 0;;j++) {
			if(*ptr == ',' || *ptr == '}') {
				head_logfile[i][j] = '\0';
				*(ptr++);
				break;
			}
			head_logfile[i][j] = *(ptr++);
		}
	}
	sort_head(head_logfile,no_of_headers);
}
/*function to add the requests into the file*/
void fill_file(char *filename, char *buff) {
	//printf("%s\n",filename);
	FILE *fp = fopen(filename,"a");
	if(fp == NULL) {
		time_t strt = time(NULL);
		while((time(NULL) - strt) <= 5) {
			sleep(0.005);
			fp = fopen(filename,"a");
			if (fp != NULL) 
				break;
		}
		if(fp == NULL) {
			perror("ERROR opening logfile even after 5 seconds");
			exit(1);
		}
	}
	time_t t = time(NULL);
	fprintf(fp,"{\n\tId: %d\n",(int)t);
	char *start;

	for(int i = 0; i < no_of_headers; i++) {
		start = strstr(buff,head_logfile[i]);
		if(start == NULL) {
			//printf("%s header not found\n",head_logfile[i]);
			continue;
		}
		char data[200];
		int i,j;
		data[0] = '\t';
		for(i = 1,j = 0; start[j] != '\r'; j++,i++){
			data[i] = start[j];}
		data[i] = '\n';
		data[i+1] = '\0';
		fputs(data,fp);
	}
	fprintf(fp,"},\n");
	fclose(fp);
}

/* function to covert an integer to a string */
char* itoa(int val, int base) {
	static char buf[32] = {0};
        int i = 30;
        for(; val && i ; --i, val /= base)
        buf[i] = "0123456789abcdef"[val % base];
        return &buf[i+1];
}

/*function for getting filename*/
void get_filename(unsigned char *buff,unsigned char *filename) {
	unsigned char *start_of_name,name[50];
	char cwd[100];
	readlink("/proc/self/exe", cwd, 100);
	char* last = strrchr(cwd,'/');
	*last = '\0';
	strcpy(filename,cwd);
	int i;
	if(strncmp(buff,"GET",3) == 0)
		start_of_name = buff+4;
	for(i = 0; start_of_name[i] != ' ';i++)
        	name[i] = start_of_name[i];
	name[i] = '\0';
	if(!strcmp(name,"/")) {
		strcat(name,"index.html");
	}
	strcat(filename,name);
}
//
int is_proxy(char *buf) {
	if(strstr(buf,"Proxy") == NULL)
		return 0;
	return 1;
}
void get_addr(char *addr, char *buf,int *port) {
	char *ptr = strstr(buf,"Proxy");
	ptr += 8;
	int i;
	for(i = 0; ptr[i] != ':'; i++)
		addr[i] = ptr[i];
	addr[i] = '\0';
	*port=0;
	i++;
	while(ptr[i]!=';'){
		int a=ptr[i]-'0';
		(*port)=(*port)*10+a;
		i++;
	}

}
//
struct header{
	char head[50];
        char value[100];
};
void getproxyheader(char* buffer,char* request,char* str) {
	int i,j,n=0,m;	
	char *start;
	start = strstr(buffer,str);
	if(start == NULL) {
		//printf("\nNo headers to be changed\n");
		return;
	}
	start += strlen(str);
	for(i = 0; start[i] != '}'; i++){
		if(start[i]==',')
			n++;
	}
	if(i!=0)
		n++;
	struct header proxyhead[n];	
	for(i=0,j=0;j<n;j++,i++){
		for(m=0;start[i]!=':';m++){
			proxyhead[j].head[m]=start[i];
			i++;
		}
		proxyhead[j].head[m]='\0';
		i++;
		for(m=0;(start[i]!=','&&start[i]!='}');m++){
			proxyhead[j].value[m]=start[i];
			i++;
		}
		proxyhead[j].value[m]='\0';
	}
	for(m=0;m<n;m++){
		//printf("%s %s\n",proxyhead[m].head,proxyhead[m].value);
	}
	char new[5000];
	for(m=0;m<n;m++){
		bzero(new,sizeof(new));
		start=strstr(request,proxyhead[m].head);
		if(start==NULL){
			//printf("\n%s  %s Header Not Found\n",str,proxyhead[m].head);
			continue;
		}
		start=strchr(start,':') + 1;
		strncpy(new,request,start-request+1);
		strcat(new,proxyhead[m].value);
		start=strchr(start,'\r');
		strcat(new,start);
		bzero(request,sizeof(request));
		strcpy(request,new);
	}
	//printf("\nmodified\n%s",request);
}

/*function to get file type*/
void get_filetype(unsigned char *name,unsigned char *type) {
	unsigned char *start_of_type = strchr(name,'.');
	start_of_type += 1;
	char ty[30];
	int i = 0;
	for(; start_of_type[i] != '\0'; i++)
		ty[i] = start_of_type[i];
	ty[i] = '\0';
	if(strcmp(ty,"html") == 0)
		strcpy(type,"text/html");
	else if(strcmp(ty,"css") == 0)
		strcpy(type,"text/css");
	else if(strcmp(ty,"png") == 0)
		strcpy(type,"image/png");
	else if(strcmp(ty,"js") == 0)
		strcpy(type,"application/javascript");
	else if(strcmp(ty,"jpg") == 0)
		strcpy(type,"image/jpg");
}

/*function to generate response*/
void gen_response(unsigned char *resp,unsigned char *name, long len) {
	unsigned char type[30];
	get_filetype(name,type);
	strcat(resp,itoa(len,10));
        strcat(resp,"\r\nContent-Type: ");
	strcat(resp,type);
	strcat(resp,"\r\nConnection: keep-alive\r\n\r\n");
}

  
void handle_sigint(int sig) 
{ 
    printf("Caught signal %d\n", sig);
    close(sockfd);
    exit(1);
}
void* handle_proxy(void *vargp) {
	int n;
	int threadno=threadcount;
	printf("\nCreated thread :%d",threadno);
	char buffer[10000];
	//signal(SIGINT, handle_sigint); 
	bzero(buffer,10000);
	n = read(*((int *)vargp),buffer,10000);
	if(n <=0) {
		perror("error reading from socket, terminating thread...");
		close(*((int *)vargp));
		return 0;
	}
	//printf("\n%s\n ",buffer);

	fill_file(logfile,buffer);
	getproxyheader(config_buf,buffer,"Proxy_Headers : {");
	int app_fd;
	struct sockaddr_in serv_add;
	app_fd = socket(AF_INET, SOCK_STREAM, 0);
	bzero((char *) &serv_add, sizeof(serv_add));
	serv_add.sin_family = AF_INET;
	if(inet_pton(AF_INET, addr_app_serv, &serv_add.sin_addr)<=0) {
		printf("\nInvalid address/ Address not supported \n");
		close(*((int *)vargp));
		exit(1);
	}
	serv_add.sin_port = htons(port_app);
	if (connect(app_fd, (struct sockaddr*)&serv_add, sizeof(serv_add)) < 0) {
		perror("ERROR connecting");
		close(*((int *)vargp));
		exit(1);
	}

	n = send(app_fd,buffer,strlen(buffer),0);
	if(n <=0) {
		perror("error writing to socket, terminating thread...");
		close(*((int *)vargp));
		return 0;
	}
	unsigned char response[3000000];
	bzero(response,3000000);
	fcntl(app_fd, F_SETFL, O_NONBLOCK); /* Change the socket into non-blocking state */
	int flag = 0;
	time_t start = time(NULL);
	while((time(NULL) - start) <= 10) {
		n = recv(app_fd,response,3000000,0);
		if (n > 0) {
			//printf("\n%s",buffer);
			flag = 1;
			break;
		}
	}
	if(flag == 0)
	{
		printf("\nrequest timed out\n");
		char timeout[1000] = "HTTP/1.1 408 Request Timeout\r\nContent-Length: 98\r\nContent-Type: text/html\r\nConection: Closed\r\n\r\n";
		strcat(timeout,"<!DOCTYPE HTML><html><body><h1>408-Request Timeout</h1><p>The request timed out</p></body></html>"); 
		n = write(*((int *)vargp),timeout,strlen(timeout));
		if(n <= 0) {
			perror("error writing to socket, terminating thread...");
			close(*((int *)vargp));
			return 0;
		}
		printf("timed out file sent");
		close(*((int *)vargp));
		return 0;
	}
	getproxyheader(config_buf,response,"Response_Headers : {");
	//printf("\n%s\n",response);
	n = send(*((int *)vargp),response,strlen(response),0);
	if(n <= 0) {
		perror("error writing to socket, terminating thread...");
		close(*((int *)vargp));
		return 0;
	}
	close(*((int *)vargp));
}
void* non_proxy(void *vargp) {
	int n;
	int threadno=threadcount;
	printf("\nCreated thread :%d with file descriptor :%d",threadno,*((int *)vargp));
	char buffer[10000];
	//signal(SIGINT, handle_sigint); 
	bzero(buffer,10000);
	n = read(*((int *)vargp),buffer,5000);
	//printf("\n%s\n",buffer);
	if(n <= 0) {
		perror("error reading from socket, terminating thread...");
		printf("(in thread%d)",threadno);
		close(*((int *)vargp));
		return 0;
	}
	fill_file(logfile,buffer);
	
	char filename[100];
	get_filename(buffer,filename);
	
	unsigned char response[300000] = "HTTP/1.1 200 OK\r\nContent-Length: ";
	unsigned char body[300000];
	FILE *fptr;
	fptr = fopen(filename,"rb");
	if(fptr == NULL) {
		perror("ERROR opening file with filename ");
		printf("%s\n404 not found sent",filename);
		//perror("ERROR opening file with filename ");
		char timeout[1000] = "HTTP/1.1 404 Not Found\r\nContent-Length: 97\r\nContent-Type: text/html\r\nConection: Closed\r\n\r\n";
		strcat(timeout,"<!DOCTYPE HTML><html><body><h1>404-file not found</h1><p>The file was not found</p></body></html>"); 
		write(*((int *)vargp),timeout,strlen(timeout));
		close(*((int *)vargp));
		return 0;
	}
	fseek(fptr,0,SEEK_END);
	long file_len = ftell(fptr);
	rewind(fptr);
	fread(body,file_len,1,fptr);
	fclose(fptr);
	body[file_len] = '\0';
	gen_response(response,filename,file_len);
	if(strstr(filename,"png") == NULL) {
		strcat(response,body);
		n = write(*((int *)vargp),response,strlen(response));
		if (n <= 0) {
			perror("ERROR writing to socket");
			//printf("\n%s %d",response,*((int *)vargp));
			close(*((int *)vargp));
			//printf("\n%s",response);
			return 0;
		}
		else {
			printf("sent to descriptor %d",*((int *)vargp));
		}
	}
	else {
		n = write(*((int *)vargp),response,strlen(response));
		if (n <= 0) {
			
			perror("ERROR writing to socket");
			
			close(*((int *)vargp));
			return 0;
		}	
		n = write(*((int *)vargp),body,file_len);
		if (n <= 0) {
			perror("ERROR writing to socket");
			//printf("\n%s",response);
			close(*((int *)vargp));
			return 0;
		}
	}
	close(*((int *)vargp));
}
int main( int argc, char *argv[] ) {
	

	
	int portno, clilen,config_len;
	struct sockaddr_in serv_addr, cli_addr;
	
	 /* First call to socket() function */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		perror("ERROR opening socket");
		exit(1);
	}
	if(argc == 1)
		strcpy(config_file,"/tmp/myconfig.cfg");
	else if(argc == 2)
		strcpy(config_file,argv[1]);
	else {
		perror("ERROR more command line inputs than needed");
		exit(1);
	}
	printf("%s",config_file);
	FILE *config = fopen(config_file,"rb");
	if(config == NULL) {
		perror("ERROR opening config file");
		exit(1);
	}
	fseek(config,0,SEEK_END);
	config_len = ftell(config);
	rewind(config);
	fread(config_buf,config_len,1,config);
	fclose(config);
	config_buf[config_len] = '\0';
	//printf("%s",config_buf);
	/*to get port no*/
	port_no(config_buf,&portno);

	/*to get logfile address */
	log_file_address(config_buf,logfile);

	/* to get the headers to be added to the log file*/
	get_log_head(config_buf);
	/* Initialize socket structure */
	bzero((char *) &serv_addr, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = inet_addr("0.0.0.0");
	serv_addr.sin_port = htons(portno);

        /* Now bind the host address using bind() call.*/
	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
 		perror("ERROR on binding");
     		exit(1);
   	}

  	/* Now start listening for the clients, here process will*/
        /* go in sleep mode and will wait for the incoming connection */
        listen(sockfd,5);
        clilen = sizeof(cli_addr);
	/*to determine if it is a proxy server or not*/
	int isproxy = is_proxy(config_buf);
	printf("%d\n",isproxy);

	while(1) {
		int cli_fd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
		if (cli_fd < 0) {
	 		perror("ERROR on accept");
			exit(1);
	   	}
		int fd = cli_fd;
		signal(SIGINT, handle_sigint);
		if(isproxy) {
			get_addr(addr_app_serv,config_buf,&port_app);
			pthread_t thread_id;
			pthread_create(&thread_id, NULL,handle_proxy, (void *)&fd);
		}
		else {
			
			pthread_t thread_id;
			pthread_create(&thread_id, NULL,non_proxy, (void *)&fd);
		}
		threadcount++;
	}
	return 0;
}
