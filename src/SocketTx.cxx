#include "SocketTx.hxx"

#include <functional>
#include <unistd.h>

#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <netdb.h>
#include <string>

#include "ReadoutUtils.h"
#include <InfoLogger/InfoLogger.hxx>
using namespace AliceO2::InfoLogger;
extern InfoLogger theLog;	// hook to global infologger handle

SocketTx::SocketTx(std::string name, std::string host, int port) {
  shutdownRequest=0;  
  clientName=name;
  serverHost=host;
  serverPort=port;
  
  isSending=0;
  currentBlock=nullptr;
  //blocks=std::make_unique<AliceO2::Common::Fifo<DataBlockContainerReference>>(100);
  
  std::function<void(void)> f = std::bind(&SocketTx::run, this);
  th=std::make_unique<std::thread>(f);
}

SocketTx::~SocketTx(){
  shutdownRequest=1;
  if (th!=nullptr) {
    th->join();
  }
  if (currentBlock!=nullptr) {
    theLog.log("%s: block sent incomplete : %lu/%u",clientName.c_str(),currentBlockIndex,currentBlock->getData()->header.dataSize);
    currentBlock=nullptr;
  }
}

void SocketTx::run() {
 
  // connect remote server
  
  int sockfd=socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd == -1) { throw __LINE__; }
  
  struct sockaddr_in servaddr; 
  bzero(&servaddr, sizeof(servaddr)); 
  servaddr.sin_family = AF_INET; 
  servaddr.sin_port = htons(serverPort);

  struct hostent *hp;
  in_addr_t inaddr;
  if ( (inaddr = inet_addr(serverHost.c_str())) != INADDR_NONE ) {
    bcopy((char *) &inaddr, (char *) &servaddr.sin_addr, sizeof(inaddr));
  } else {
    hp = gethostbyname(serverHost.c_str());
    if (hp == NULL) { throw __LINE__; }
    bcopy(hp->h_addr, (char *) &servaddr.sin_addr, hp->h_length);
  }

  if (connect(sockfd, (struct sockaddr*) &servaddr, sizeof(servaddr)) < 0) {
    theLog.log("%s: failure connecting: %s",clientName.c_str(),strerror(errno));
    close(sockfd);
    return;
  }

  char h[100];
  gethostname(h,100);
  clientName+= std::string(" @ ") + h + " -> " + serverHost + ":" + std::to_string(serverPort);
  theLog.log("%s connected",clientName.c_str());
  
  // loop: send current block, if any
  for(;;) {
    if (isSending) {
      size_t cs=currentBlock->getData()->header.dataSize-currentBlockIndex;
      int n=write(sockfd, &currentBlock->getData()->data[currentBlockIndex], cs);
      if (n==0) {break;}
      if (n>0) {
	bytesTx+=n;
	currentBlockIndex+=n;
	if (currentBlockIndex==currentBlock->getData()->header.dataSize) {
	  currentBlock=nullptr;
          isSending=0;
	}
      }
      if (n<0) {break;}
    } else {
      usleep(10);
    }
  
    if (shutdownRequest) {
      break;
    }
  }

  // cleanup & log stats
  
  close(sockfd);
  theLog.log("%s : written %llu bytes",clientName.c_str(),bytesTx);

  double t0=t.getTime();
  double rate=(bytesTx/t0);
  
  theLog.log("%s : data: %s in %.2fs",clientName.c_str(),NumberOfBytesToString(bytesTx,"bytes",1024).c_str(),t0);
  theLog.log("%s : rate: %s",clientName.c_str(),NumberOfBytesToString(rate*8,"bps").c_str());
}

int SocketTx::pushData(DataBlockContainerReference &b) {
  if (isSending) {
    // there's already a block queued
    return -1;
  }
  
  // queue block
  currentBlockIndex=0;
  currentBlock=b;
  isSending=1;
  
  return 0;
}