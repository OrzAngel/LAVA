/* 
This code primarily comes from 
http://www.prasannatech.net/2008/07/socket-programming-tutorial.html
and
http://www.binarii.com/files/papers/c_sockets.txt
 */
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string>
#include <iostream>
#include <pthread.h>
#include <string>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <termios.h>

#include <thread>         
#include <mutex>
#include <sstream>

#include <queue>

using namespace std;
 

bool isRunning = true;    // if the server should be running
bool isF = true;         // if the current unit is Fahrenheit
int isConnect = 1;    // 1 means the server is connected to Arduino, 0 otherwise


int fd_a;
int fd_p;

deque<double> temp;       // temperature deque
double low = 100;         // lowest temp
double high = -10;        // highest temp
mutex mtx;                // a lock

int threshold = -1;
bool isThreshHoldCelsius = true;
bool sentError = false;


bool disc1 = false;
bool disc2 = false;


void* user_input(void *arg){      // waiting for input and after getting input kill the server
  pthread_t s = *(pthread_t*)arg;
  int j = 0;

  string input; 
  while(true){
    getline(cin, input);
    if (input == "q") {     // if the input is q, close the server
      isRunning = false;    // set isRunning to false to end the server
      pthread_cancel(s);
      break;
    }
  }

  return NULL;
}

void configure(int fd) {      // configure the USB port
  struct  termios pts;
  tcgetattr(fd, &pts);
  cfsetospeed(&pts, 9600);   
  cfsetispeed(&pts, 9600);   
  tcsetattr(fd, TCSANOW, &pts);
}

void* timer(void *arg){
	disc2 = false;
	sleep(7);
	disc2 = true;
}

void* readUSB(void *arg){       // read from Arduino 
  while (true){
  char* file_name = (char*)(arg);
  pthread_t p_timer;
  fd_a = open(file_name, O_RDWR | O_NOCTTY | O_NDELAY);
  
  if (fd_a < 0) {
    //perror("Could not open file");
    continue;
  }
  else {
    cout << "Successfully opened " << file_name << " for reading/writing" << endl;
  	disc1 = false;
  	disc2 = false;
  }

  configure(fd_a);

  int i = 0;
  while (true){       // keep looping

      i++;
      char buffer[100];
      int bytes_read = read(fd_a, buffer, 99);

      if (bytes_read == -1){
      	if (!disc1){
      		disc1 = true;
      		if (pthread_create(&p_timer, NULL, timer, NULL) != 0) {     // create the thread for listening user input
        		perror("pthread_create");
        		exit(1);
  			}

      	} else {
      		if (disc2){
      			cout << "DISCONNECT FROM ARDUINO" << endl;
      			isConnect = false;
      			close(fd_a);
      			break;
      		} 
      	}
      	continue;
      }

      if (disc1){
      	pthread_cancel(p_timer);
      }
      disc1 = false;
      disc2 = false;
      isConnect = true;


      if (bytes_read == 0) {

        continue;
      } 
      
      
      string str = "";

      int tmp = bytes_read;
      while (buffer[bytes_read - 1] != '\n'){     // handle incomplete msg, keep reading until a null terminator is met.
        buffer[bytes_read] = '\0';
        str += buffer;
        
        for (int i = 0; i < 100; i++){
          buffer[i] = '\0';
        }

        bytes_read = read(fd_a, buffer, 99);  
        if (bytes_read == -1){
          bytes_read = tmp;
        } else {
          tmp = bytes_read;
        }
      }
      
      str += buffer;

      for (int i = 0; i < 100; i++){
        buffer[i] = '\0';
      } 

      if (str.size() >= 26){        // the module that reads in data and push to queue

        string test = str.substr(19, 7);
        double t = atof(test.c_str());

        if (t > high && t < 50) high = t;
        if (t < low && t > 0) low = t;

        if (t > 50 || t < 10){
        	disc1 = false;
        	disc2 = false;
            continue;
        }

        if (t > 33 || t < 20){
        	disc1 = false;
        	disc2 = false;
            continue;
        }

        if (temp.size() < 3600){    // if the size is equal or less than 3600
            cout << t << endl;
            temp.push_back(t);
            disc1 = false;
        	disc2 = false;
        } else {
            temp.pop_front();
            cout << t << endl;
            temp.push_back(t);
            disc1 = false;
        	disc2 = false;
        }
      } else {
      	disc1 = false;
        disc2 = false;
      }

      if (str == ""){
      	disc1 = false;
        disc2 = false;
        continue;
      } else {
        disc1 = false;
        disc2 = false;
      }
      
    }
  }	
}

int requestHandler(char request[]){   // 0 for stats, 1 for F,C switch, 3 for standby, 4 for resume
    string tmp = "";
    int i = 0;
    while(request[i] != '\0'){
      tmp += request[i];
      i++;
    }

    int index = tmp.find("/595/");      
    string relativePath = tmp.substr(index+5, 1);     // get the code for the operation

    if (relativePath == "0") return 0;
    if (relativePath == "1") {
    	return 1;
    }
    if (relativePath == "2") {

      string warningTemp = tmp.substr(index+7, 3);

      if (warningTemp[2] == 'f'){
          isThreshHoldCelsius = false;
      } else {
          isThreshHoldCelsius = true;
      }
      warningTemp = warningTemp.substr(0, 2);

      threshold = stoi(warningTemp);
      return 2;
    }
    if (relativePath == "3") return 3;
    if (relativePath == "4") return 4;
    if (relativePath == "q") return 10;
    return -1;
}

string DtoS(double t){
    string ret = to_string(t).substr(0, 5);
    return ret;
}

void statsHandler(int fd){          // send stats to the pebble

    if (temp.size() == 0) return;
    deque<double>::iterator itr;
    double sum = 0;
    int count = 0;
    for (itr = temp.begin(); itr != temp.end(); itr++){     // calculate avg
        count++;
        sum += (*itr);
    }
    double avg = sum / count;

    mtx.lock();
    double recent = temp.back();    // the most recent reading
    mtx.unlock();


    if (!isF){
    	string reply = "{\n\"low\": \"" + DtoS(low) + "C\", \"high\": \"" + DtoS(high) + "C\", \"avg\": \"" + DtoS(avg) + "C\", \"recent\": \"" + DtoS(recent) + "C\", \"isConnect\": \"" + to_string(isConnect) + "\"\n}\n";
      cout << reply << endl;
    	send(fd, reply.c_str(), reply.length(), 0);
    } else {
    	string reply = "{\n\"low\": \"" + DtoS(low * 1.8 + 32) + "F\", \"high\": \"" + DtoS(high * 1.8 + 32) + "F\", \"avg\": \"" + DtoS(avg * 1.8 + 32) + "F\", \"recent\": \"" + DtoS(recent * 1.8 + 32) + "F\", \"isConnect\": \"" + to_string(isConnect) + "\"\n}\n";
      cout << reply << endl;
    	send(fd, reply.c_str(), reply.length(), 0);
    }
    close(fd);
}

void FCHandler(int fd){     // handle FC switch
	isF = !isF;

    deque<double>::iterator itr;
    double sum = 0;
    int count = 0;
    for (itr = temp.begin(); itr != temp.end(); itr++){
        count++;
        sum += (*itr);
    }
    double avg = sum / count;

    mtx.lock();
    double recent = temp.back();
    mtx.unlock();

    if (!isF){
    	string reply = "{\n\"low\": \"" + DtoS(low) + "C\", \"high\": \"" + DtoS(high) + "C\", \"avg\": \"" + DtoS(avg) + "C\", \"recent\": \"" + DtoS(recent) + "C\", \"isConnect\": \"" + to_string(isConnect) + "\"\n}\n";
      cout << reply << endl;
    	send(fd, reply.c_str(), reply.length(), 0);
    } else {
    	string reply = "{\n\"low\": \"" + DtoS(low * 1.8 + 32) + "F\", \"high\": \"" + DtoS(high * 1.8 + 32) + "F\", \"avg\": \"" + DtoS(avg * 1.8 + 32) + "F\", \"recent\": \"" + DtoS(recent * 1.8 + 32) + "F\", \"isConnect\": \"" + to_string(isConnect) + "\"\n}\n";
      cout << reply << endl;
    	send(fd, reply.c_str(), reply.length(), 0);
    }
    close(fd);

    if (isF){
      char* res = "f";

      write(fd_a, res, 10);
    } else {
      char* res = "c";

      write(fd_a, res, 10);
    }

}


void StandbyHandler(int fd){      // handles standby

    deque<double>::iterator itr;
    double sum = 0;
    int count = 0;
    for (itr = temp.begin(); itr != temp.end(); itr++){
        count++;
        sum += (*itr);
    }
    double avg = sum / count;

    mtx.lock();
    double recent = temp.back();
    mtx.unlock();

    if (!isF){
      string reply = "{\n\"low\": \"" + DtoS(low) + "C\", \"high\": \"" + DtoS(high) + "C\", \"avg\": \"" + DtoS(avg) + "C\", \"recent\": \"" + DtoS(recent) + "C\", \"isConnect\": \"" + to_string(isConnect) + "\"\n}\n";
      cout << reply << endl;
      send(fd, reply.c_str(), reply.length(), 0);
    } else {
      string reply = "{\n\"low\": \"" + DtoS(low * 1.8 + 32) + "F\", \"high\": \"" + DtoS(high * 1.8 + 32) + "F\", \"avg\": \"" + DtoS(avg * 1.8 + 32) + "F\", \"recent\": \"" + DtoS(recent * 1.8 + 32) + "F\", \"isConnect\": \"" + to_string(isConnect) + "\"\n}\n";
      cout << reply << endl;
      send(fd, reply.c_str(), reply.length(), 0);
    }
    close(fd);

    char* res = "s";
    write(fd_a, res, 10);

}


void ResumeHandler(int fd){       // handles resume

    deque<double>::iterator itr;
    double sum = 0;
    int count = 0;
    for (itr = temp.begin(); itr != temp.end(); itr++){
        count++;
        sum += (*itr);
    }
    double avg = sum / count;

    mtx.lock();
    double recent = temp.back();
    mtx.unlock();

    if (!isF){
      string reply = "{\n\"low\": \"" + DtoS(low) + "C\", \"high\": \"" + DtoS(high) + "C\", \"avg\": \"" + DtoS(avg) + "C\", \"recent\": \"" + DtoS(recent) + "C\", \"isConnect\": \"" + to_string(isConnect) + "\"\n}\n";
    
      send(fd, reply.c_str(), reply.length(), 0);
    } else {
      string reply = "{\n\"low\": \"" + DtoS(low * 1.8 + 32) + "F\", \"high\": \"" + DtoS(high * 1.8 + 32) + "F\", \"avg\": \"" + DtoS(avg * 1.8 + 32) + "F\", \"recent\": \"" + DtoS(recent * 1.8 + 32) + "F\", \"isConnect\": \"" + to_string(isConnect) + "\"\n}\n";
    
      send(fd, reply.c_str(), reply.length(), 0);
    }
    close(fd);

    sentError = false;
    char* res = "r";
    write(fd_a, res, 10);
    write(fd_a, res, 10);

}

void warningHandler(int fd){        //  to be finished!!!!!!!
  string reply;

  mtx.lock();
  double recent = temp.back();    // the most recent reading
  mtx.unlock();

  if ((isThreshHoldCelsius && recent > threshold) || (!isThreshHoldCelsius && (recent * 1.8 + 32 > threshold))){

    reply = "{\n\"isWarning\": \"1\"\n}\n";
    send(fd, reply.c_str(), reply.length(), 0);
    cout << reply << endl;

    if (!sentError){
      char* res = "e";
      write(fd_a, res, 10);
      sentError = true;
    }
  } else {
    reply = "{\n\"isWarning\": \"0\"\n}\n";
    cout << reply << endl;
    send(fd, reply.c_str(), reply.length(), 0);
  }
  close(fd);
  
}

void* start_server(void* PN){
	int PORT_NUMBER = *(int*)PN;
	cout << PORT_NUMBER << endl;

	// structs to represent the server and client
      struct sockaddr_in server_addr,client_addr;    
      
      int sock; // socket descriptor

      // 1. socket: creates a socket descriptor that you later use to make other system calls
      if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
	       perror("Socket");
	       exit(1);
      }
      int temp;
      if (setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&temp,sizeof(int)) == -1) {
      	 perror("Setsockopt");
      	 exit(1);
      }

      // configure the server
      server_addr.sin_port = htons(PORT_NUMBER); // specify port number
      server_addr.sin_family = AF_INET;         
      server_addr.sin_addr.s_addr = INADDR_ANY; 
      bzero(&(server_addr.sin_zero),8); 
      
      // 2. bind: use the socket and associate it with the port number
      if (bind(sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1) {
	       perror("Unable to bind");
	       exit(1);
      }

      // 3. listen: indicates that we want to listn to the port to which we bound; second arg is number of allowed connections
      if (listen(sock, 5) == -1) {
	       perror("Listen");
	       exit(1);
      }
          
      // once you get here, the server is set up and about to start listening

      cout << endl << "Server configured to listen on port " << PORT_NUMBER << endl;
      fflush(stdout);
     

     while (isRunning){

      // 4. accept: wait here until we get a connection on that port
      int sin_size = sizeof(struct sockaddr_in);
      int fd = accept(sock, (struct sockaddr *)&client_addr,(socklen_t *)&sin_size);
      cout << "Server got a connection from " << inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port) << endl;
      
      // buffer to read data into
      char request[1024];
      
      // 5. recv: read incoming message into buffer
      int bytes_received = recv(fd,request,1024,0);
      // null-terminate the string
      request[bytes_received] = '\0';
      //cout << "Here comes the message:" << endl;
      cout << request << endl;
      

      int ret = requestHandler(request);



      if (ret == 0){
          statsHandler(fd);
      } else if (ret == 2){
          warningHandler(fd);
      } else if (ret == 1){
          FCHandler(fd);
      } else if (ret == 3){
          StandbyHandler(fd);
      } else if (ret == 4){
          ResumeHandler(fd);
      } else if (ret == 10){
      	  break;
      }


    }
      close(sock);
      cout << "Server closed connection" << endl;
  
      return 0;
}


int main(int argc, char *argv[])
{
  // check the number of arguments
  if (argc != 3)
    {
      cout << endl << "Usage: server [port_number] [serial_port USB]" << endl;
      exit(0);
    }

  int PORT_NUMBER = atoi(argv[1]);
  char* file_name = argv[2];

  pthread_t server;
  if (pthread_create(&server, NULL, start_server, &PORT_NUMBER) != 0) {     // create the thread for listening user input
        perror("pthread_create");
        exit(1);
  }

  pthread_t tid_typing;
  if (pthread_create(&tid_typing, NULL, user_input, &server) != 0) {     // create the thread for listening user input
        perror("pthread_create");
        exit(1);
  }

  pthread_t tid_USB;
  if (pthread_create(&tid_USB, NULL, readUSB, file_name) != 0) {      // create the thread for reading USB input
        perror("pthread_create");
        exit(1);
  }

  pthread_join(tid_typing, NULL);
  cout << "Goodbye CHRIS" << endl;
}

