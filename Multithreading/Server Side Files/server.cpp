#include "server.h"

server::server(const char* port, device* init_Devices)
{
	std::cout << "Server constructor started.\n" << std::flush;

	memset(&hints, 0, sizeof(hints)); 
	
	hints.ai_family = AF_UNSPEC; 
	hints.ai_socktype = SOCK_STREAM; 
	hints.ai_flags = AI_PASSIVE; 

	std::cout << "Getting possible host information values...\n" << std::flush;

	do
	{
		status = getaddrinfo(NULL, port, &hints, &serverInfo);

		if (status == INVALID)
		{
			if (errno  == EAI_AGAIN)
			{
		
				continue;
			}
			else 
			{

			std::cout << "getaddrinfo error: " << gai_strerror(status) << "\n" << std::flush;	
			exit(EXIT_FAILURE);
		
			}
		
		}

  
	}while(status != 0);

	std::cout << "Acquired possible host information values.\n" << std::flush;

	std::cout << "Attempting to create listener socket...\n" << std::flush;

	

	for (addrinfo* p = serverInfo; p != nullptr; p = p->ai_next)
	{

		listener = socket(serverInfo->ai_family, serverInfo->ai_socktype, serverInfo->ai_protocol);
		if (listener == INVALID)
		{
			perror("listener socket");
			std::cout << "Trying next available configuration...\n" << std::flush;
	
			continue;
	

		}
	
		std::cout << "Succesfully created a listener socket.\n" << std::flush;  

		int yes = 1;
	
		std::cout << "Setting listener port to be reuseable...\n" << std::flush;

		if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == INVALID) 
		{		
			perror("setsockopt");
			exit(EXIT_FAILURE);
		}
	

		std::cout << "Listener port is now reuseable.\n" << std::flush;

		std::cout << "Binding listener to port: " << port << ".\n" << std::flush;

		if(bind(listener, serverInfo->ai_addr, serverInfo->ai_addrlen) == INVALID)
		{

			close(listener);

			perror("listener bind");
			std::cout << "Trying next available configuration...\n" << std::flush;

			continue;
		}

		std::cout << "Listener has been bound to port: " << port << ".\n" << std::flush; 

		break;
	
	}		
	
	if(listener == INVALID)
	{
		
		std::cout << "Was unable to find a valid socket configuration. Aborting program...\n" << std::flush;
		exit(EXIT_FAILURE);
	
	}

	freeaddrinfo(serverInfo);
	
	std::cout << "Loading device info...\n" << std::flush;


	devices = init_Devices;

	std::cout << "Device info loaded.\n" << std::flush;


	std::cout << "Server constructor finished.\n" << std::flush;
}

int server::sendall(Socket sock, const char* msg, int* len)
{	
	int total_Sent = 0;
	int bytes_Left = *len;
	int bytes_Sent = 0;


	char buf;

	std::cout << "Attempting to send " << *len << " bytes of information.\n" << std::flush;

	while(total_Sent < *len)
	{	
		
		bytes_Sent = send(sock, (msg + bytes_Sent), bytes_Left, 0);
		
		if(bytes_Sent == INVALID)
		{
			
			if (errno == EPIPE)
			{
				std::cout << "Client has closed connection on their end. Send aborted. Closing socket...\n" << std::flush;

			}
			
			{

				perror("send");

			}


			for(int i = 0; i < NUM_DEVICES; i++)
			{
				if (sock == devices[i].connection)
				{
					close(sock);
					devices[i].connection = INVALID;  
					std::cout << "Socket to " << devices[i].alias << " has been closed.\n" << std::flush;
				} 

			}

		

			break;
		} 

		total_Sent += bytes_Sent;
		bytes_Left -= bytes_Sent;
	}
	

	
	send(sock, terminator, 1, 0);


	std::cout << "Sent " << total_Sent << " bytes out of " << *len << " bytes.\n" << std::flush;

	*len = bytes_Sent;	

	return (bytes_Sent == INVALID) ? -1 : 0;
	
}

std::string server::recvall(Socket sock)
{
	std::string msg;
	char temp[10];
	std::cout << "Awaiting transmission...\n" << std::flush;

	while(true)
	{	
		static bool transmitting = false;
		//std::this_thread::sleep_for(std::chrono::milliseconds(500));

		status = recv(sock, (void*)temp, 1, 0);

		if (status != INVALID)
		{	

			if (!transmitting)
			{
				std::cout << "Incoming transmission.\n" << std::flush;
				transmitting = true;
			}
		
			std::string tempString(temp);
			msg += tempString;
			
			//std::cout << msg.at(msg.length()) << std::endl;
			std::cout << "Recieved a new packet. Appended " << status << " bytes to the message.\n" << std::endl;		

			if (msg.at(msg.length() - 1) == '`' && msg.length() > 0)
			{

				std::cout << "Termination character detected. All packets recieved.!\n" << std::flush;
				msg.pop_back();	
				break;	
			}

		}
		else
		{
			perror("recvall");
			exit(EXIT_FAILURE);
		}

			
	
	}
	
	

	return msg;
	
}

void server::queue()
{
	std::cout << "Listening on the listener...\n" << std::flush;
	while(!end)
	{
		status = listen(listener, 10);
		
		if (status == INVALID)
		{
			perror("listen");
			exit(EXIT_FAILURE);
		}

		acceptInfo = (sockaddr*)&stg_acceptInfo;
		stg_acceptLen = sizeof(stg_acceptInfo);
	
		unidentified = accept(listener, acceptInfo, &stg_acceptLen);
		std::cout << "Found an unidentified connection...\n" << std::flush;

		if (unidentified == INVALID)
		{
			perror("accept");
			exit(EXIT_FAILURE);
		}
		
		
		if (acceptInfo->sa_family == AF_INET) 
		{
			char IP[INET_ADDRSTRLEN];
			match((char*)&IP, sizeof(IP));	

		}
		else if (acceptInfo->sa_family == AF_INET6)
		{	
			char IP[INET6_ADDRSTRLEN];
			match((char*)&IP, sizeof(IP));
		}
		else
		{
			close(unidentified);		
			std::cout << "Invalid sa_family. Connection closed.\n" << std::flush;
		}	
	}

	
	
}

void server::match(char* ip, size_t size)
{
	
	inet_ntop(acceptInfo->sa_family, get_in_addr(acceptInfo), ip, size);
	uint16_t service = ntohs(get_in_service(acceptInfo));
	
	if (acceptInfo->sa_family == AF_INET)
	{
		std::cout << "Unidentified connection to: " << ip << ":" << service << "\n" <<  std::flush;
	}
	else if (acceptInfo->sa_family == AF_INET6)
	{	
		std::cout << "Unidentified connection to: [" << ip << "]:" << service << "\n" << std::flush;
	}

	for (int i = 0; i < NUM_DEVICES; i++)
	{
		
		if (strcmp(ip, devices[i].IP.c_str()) == 0) 
		{
			devices[i].connection = unidentified;
			unidentified = 0;

			std::cout << "Matched " << ip << " to " << devices[i].alias << "\n" << std::flush;				
		}

		return;
	}

	std::cout << "Unable to match " << ip << " with any known devices. Closing connection...\n" << std::flush;

	close(unidentified); 

	std::cout << "Connection with " << ip << " closed.\n" << std::flush;	

}

void* server::get_in_addr(sockaddr* sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((sockaddr_in*)sa)->sin_addr);
    }

    return &(((sockaddr_in6*)sa)->sin6_addr);
}

uint16_t server::get_in_service(sockaddr* sa)
{
    if (sa->sa_family == AF_INET) {
        return ((sockaddr_in*)sa)->sin_port;
    }

    return ((sockaddr_in6*)sa)->sin6_port;
}

server::~server()
{
	std::cout << "Server destructor started.\n" << std::flush;

	std::cout << "Closing listener...\n" << std::flush;

	close(listener);
	
	std::cout << "Listner closed.\n" << std::flush;

	

	for(int i = 0; i < NUM_DEVICES; i++)
	{
		std::cout << "Closing device \"" << devices[i].alias << "\"...\n" << std::flush;
		close(devices[i].connection);
		std::cout << "Device " << devices[i].alias << " closed.\n" << std::flush;
	}


}
