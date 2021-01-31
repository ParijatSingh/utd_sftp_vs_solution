#include <algorithm>
#include <string>
#include <sstream>
#include <map>
#include <stdio.h>
#include <chrono>
#include <winsock.h>    /* for socket(),... */
//#include <winsock2.h> 
//#include <ws2tcpip.h>
#include <stdlib.h>
//#define _OE_SOCKETS
#include <stdint.h>
#include "packet.h"
#include "common_functions.h"

#pragma comment(lib,"WS2_32")

using namespace std::chrono;

int timeout = 2000;
bool TRACE = false;

/* Error handling function */
void HandleError(const char* errorMessage) {
    printf(errorMessage);
}


/* Function to get directory listing. 
   Sends LISTDIRREQ to server for dir @dirName. Direcroty name can be empty.
   It is upto server to decide wich directory it will list in such a case.
   Server responds with LISTDIRRES messages. Each message will have one entry of the list.
   Packet Format for LISTDIRREQ:

     2 bytes      0-512 bytes                                1 byte
    ---------------------------------------------------------------
    | MSGCODE | FILENAME                                       |\0|
    ---------------------------------------------------------------
    MSGCODE = 01; DIR/FILE = 'D' or 'F'

   Packet Format for LISTDIRRES:

     2 bytes    1 byte     2 bytes         0-256 bytes        1 byte
    ---------------------------------------------------------------
    | MSGCODE | DIR/FILE | FILESIZE(KB) | FILENAME             |\0|
    ---------------------------------------------------------------
    MSGCODE = 01; DIR/FILE = 'D' or 'F'

    Client waits until timeout if no more messages coming from server.
*/
void listDir(const char* dirName, int sock, sockaddr_in serverAddr)
{
    if(TRACE) printf("Inside listDir\n");

    ListDirReq req;
    req.msgcode = htons(LISTDIRREQ);
    strcpy_s(req.dirName, dirName);
    int structSize = strlen(req.dirName) + 2 + 1; // 2bytes msgcode, 1  sizeof(req); // 
    int byttessent = sendto(sock, (char*)&req, structSize, 0, (struct sockaddr*)&serverAddr,sizeof(serverAddr));
    if(byttessent != byttessent)   HandleError("actual bytes sent does not match with intended bytes");

    if (TRACE) printf("LISTDIRREQ Request sent. Bytes sent:%d\n", byttessent);

    int recvMsgSize;
    char msgBuffer[MSGMAXSIZE];
    struct sockaddr_in senderAddr;
    int senderLen = sizeof(senderAddr);
    //set socket timeout
    struct timeval read_timeout;
    read_timeout.tv_sec = 0;
    read_timeout.tv_usec = 1000000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*) &read_timeout, sizeof read_timeout);
    milliseconds begin = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()
    );
    for (;;)
    {
        if ((recvMsgSize = recvfrom(sock, msgBuffer, MSGMAXSIZE, 0,
            (struct sockaddr*)&senderAddr, &senderLen)) < 0)
           // HandleError("recvfrom() failed");
        {
            if (WSAGetLastError() == WSAEWOULDBLOCK) {
                if ((duration_cast<milliseconds>(
                    system_clock::now().time_since_epoch()
                    ).count() - begin.count()) > 2000) { // 2 seconds timeout
                    std::cout << std::endl;
                    break;
                }
            }
            else {
                printf("Server: Some error encountered with code number : % ld\n", WSAGetLastError());
            }
        }
        else {
            Packet* pkt = reinterpret_cast<Packet*>(msgBuffer);
            pkt->msgcode = ntohs(pkt->msgcode);
            if (pkt->msgcode == ERR) {
                std::cout << "ERROR:\t\t" << pkt->data << std::endl << std::endl;
                break;
            }
            else if (pkt->msgcode == LISTDIRRES)
            {
                ListDirRes* listdirresponse = reinterpret_cast<ListDirRes*>(msgBuffer);
                if (strlen(listdirresponse->fileName) == 0) break;
                listdirresponse->size = ntohs(listdirresponse->size);
                if (listdirresponse->dir_or_file[0] == 'D')
                    std::cout << "<DIR>\t\t" << listdirresponse->fileName << std::endl;
                else
                    std::cout << "\t" << listdirresponse->size << "kb\t" << listdirresponse->fileName << std::endl;
                begin = duration_cast<milliseconds>(
                    system_clock::now().time_since_epoch()
                    );
            }            
        }
    }
    //std::cout << "Out of Loop" << std::endl;
}

/* function to download a list of files.
   Send GETFILEREQ to server for each file. Server responds with GETFILERES messages. 
   Each response has a uniques server transaction ID to support concurrent file downloads by multiple clients.
   Server will start sending data packets immediately after sending GETFILERES. 
   The data packet has server transaction id that will identify the file to which the client must write the data.
   At TIMEOUT the file will be closed. DATAACK is not implemented. 

   Packet GETFILEREQ Format:
     2 bytes     1-512 bytes                       1 byte
    ------------------------------------------------------
    | MSGCODE |  FILENAME                             |\0|
    ------------------------------------------------------
    MSGCODE = 03;

   Packet GETFILERES Format:
     2 bytes    2 byte        0-256 bytes           1 byte
    ------------------------------------------------------
    | MSGCODE | TRANS ID | FILENAME                   |\0|
    ------------------------------------------------------
    MSGCODE = 04;
*/

void getFile(std::vector<std::string> files, const char* outputDir, int sock, sockaddr_in serverAddr)
{
    std::map<uint16_t, std::ofstream*> tx_Fd_Map;
    int filesToGet = 0;
    std::ofstream ofs;
    for (auto& it : files) {
        GetFileReq req;
        req.msgcode = htons(GETFILEREQ);
        strcpy_s(req.fileName, it.c_str());
        int structSize = sizeof(req);
        //int structSize = strlen(req.fileName) + 2 + 1; // 2bytes msgcode, 1 for NULL '\0'
        int byttessent = sendto(sock, (char*)&req, structSize, 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
        if(byttessent != structSize)  HandleError("actual bytes sent does not match with intended bytes"); 
        filesToGet++;
    }
    int recvMsgSize;
    char msgBuffer[MSGMAXSIZE];
    struct sockaddr_in senderAddr;
    int senderLen = sizeof(senderAddr);
    struct timeval read_timeout;
    read_timeout.tv_sec = 0;
    read_timeout.tv_usec = 1000000;    
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&read_timeout, sizeof read_timeout);
    milliseconds begin = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()
        );
    for (;;)
    {
        recvMsgSize = recvfrom(sock, msgBuffer, MSGMAXSIZE, 0, (struct sockaddr*)&senderAddr, &senderLen);
        if (recvMsgSize < 0)
            // HandleError("recvfrom() failed");
        {
            if (WSAGetLastError() == WSAEWOULDBLOCK) {
                if ((duration_cast<milliseconds>(
                    system_clock::now().time_since_epoch()
                    ).count() - begin.count()) > timeout) {
                    // Close all the files when timeout happens
                    std::cout << std::endl;
                    break;
                }
            }
            else {
                printf("Server: Some error encountered with code number : % ld\n", WSAGetLastError());
            }
        }
        else {
            Packet* pkt = reinterpret_cast<Packet*>(msgBuffer);
            pkt->msgcode = ntohs(pkt->msgcode);
            if (pkt->msgcode == GETFILERES)
            {
                GetFileRes* resp = reinterpret_cast<GetFileRes*>(msgBuffer);
                resp->txId = ntohs(resp->txId);
                std::string fileName = extractFilename(resp->fileName);
                std::string oDir = outputDir;
                oDir.append("/");
                std::string path = oDir + fileName;
                int len = path.length();
                tx_Fd_Map.insert(std::pair<uint16_t, std::ofstream*>(resp->txId, new std::ofstream(path, std::ios::binary | std::ios::out)));
                std::cout << "\nfetching file ->" << resp->fileName << std::endl;
            }
            else if (pkt->msgcode == ERR) {
                std::cout << "ERROR:\t\t" << pkt->data << std::endl << std::endl;
                filesToGet--;
                //errorRes* resp = reinterpret_cast<errorRes*>(msgBuffer);
            }
            else if (pkt->msgcode == DATA) {
                Data* resp = reinterpret_cast<Data*>(msgBuffer);
                resp->txId = ntohs(resp->txId);
                resp->block_number = ntohs(resp->block_number);
                if(TRACE) std::cout << resp->block_number << " " ;

                std::map<uint16_t, std::ofstream*>::iterator it = tx_Fd_Map.find(resp->txId);
                if (it != tx_Fd_Map.end())
                {
                    //filestream found;
                    writeFileBytes(*(it->second), resp->data, recvMsgSize - 6, DATABLOCK * (resp->block_number - 1));
                    if (recvMsgSize - 6 < DATABLOCK)
                    {
                        (*(it->second)).close();
                        tx_Fd_Map.erase(resp->txId);
                        filesToGet--;
                    }
                }

                //send acknowledgement
                DataAck ack = {htons(ACK),resp->txId,resp->block_number};
                sendto(sock, (char*)&ack, sizeof(ack), 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
            }          

            //std::cout << ntohs(resp->size) << " kb\t" << fromChar->data << std::endl;
            begin = duration_cast<milliseconds>(
                system_clock::now().time_since_epoch()
                );
        }
        if (filesToGet == 0) break;
    }
    std::cout << std::endl << std::endl;
}

/* function to upload a list of files to server.
   Sends POSTFILEREQ to server for each file. Server responds with POSTFILERES messages.
   Each response has a uniques server transaction ID to support concurrent file uploads by multiple clients.
   Client will start sending data packets immediately after getting POSTFILERES.
   The data packet has server transaction id that will identify the file to which the server must write the data.

   POSTFILEREQ has client transaction ID. This is not used now. In another design option , the server instead of sending its 
   own unique transaction ID, can keep track of client transaction ID and Client IP address and port to uniquely identify
   which file to write to.

   Packet POSTFILEREQ Format:
     2 bytes    2 byte        0-512 bytes           1 byte
    ------------------------------------------------------
    | MSGCODE | TRANS ID | FILENAME                   |\0|
    ------------------------------------------------------
    MSGCODE = 05;

   Packet POSTFILERES Format:
     2 bytes    2 byte        0-256 bytes           1 byte
    ------------------------------------------------------
    | MSGCODE | TRANS ID | FILENAME                   |\0|
    ------------------------------------------------------
    MSGCODE = 06;
*/
void postFile(std::vector<std::string> files, const char* dirPath, int sock, sockaddr_in serverAddr)
{
    uint16_t transactionId = 0;
    std::map<uint16_t, std::ofstream*> tx_Fd_Map;
    std::ofstream ofs;
 NEXTFILE:   for (auto& it : files) {
        std::string filename = it.c_str();
        std::ifstream ifs(filename, std::ios::ate|std::ios::binary);
        if (ifs.is_open()) {
            // create new getfileresponse struct. add unique trancation id for each new file request
            PostFileReq req = { htons(POSTFILEREQ), htons(++transactionId) };
            strcpy_s(req.fileName, filename.c_str());
            strcpy_s(req.dirPath, dirPath);
            size_t structlength = sizeof(req);
            if (sendto(sock, (char*)&req, structlength, 0, (struct sockaddr*)&serverAddr,
                sizeof(serverAddr)) != structlength)
                HandleError("actual bytes sent does not match with intended bytes");

            char msgBuffer[MSGMAXSIZE];
            struct sockaddr_in senderAddr;
            int senderLen = sizeof(senderAddr);
            int recvMsgSize = recvfrom(sock, msgBuffer, MSGMAXSIZE, 0, (struct sockaddr*)&senderAddr, &senderLen);
            int transactionId = 0;
            for (;;)
            {
                recvMsgSize = recvfrom(sock, msgBuffer, MSGMAXSIZE, 0, (struct sockaddr*)&senderAddr, &senderLen);

                if (recvMsgSize > 0)
                {
                    Packet* pkt = reinterpret_cast<Packet*>(msgBuffer);
                    pkt->msgcode = ntohs(pkt->msgcode);
                    if (pkt->msgcode == POSTFILERES)
                    {
                        PostFileRes* resp = reinterpret_cast<PostFileRes*>(msgBuffer);
                        resp->txId = ntohs(resp->txId);
                        transactionId = resp->txId;
                        std::string fileName = extractFilename(resp->fileName);
                        printf("\nReceived transactionId: [%d] for file %s\n", resp->txId, resp->fileName);
                        break;
                    }
                    else if (pkt->msgcode == ACK) {
                        DataAck* ack = reinterpret_cast<DataAck*>(msgBuffer);
                        if(TRACE) printf("ACK %d[%d], ", ntohs(ack->txId), ntohs(ack->block_number));
                        // acknowledgement not implemented yet
                    }
                    else if (pkt->msgcode == ERR) {
                        std::cout << "ERROR:\t\t" << pkt->data << std::endl << std::endl;
                        goto NEXTFILE;
                        //errorRes* resp = reinterpret_cast<errorRes*>(msgBuffer);
                    }
                }
            }
            //after getting the POSTFILERES, start sending FILEDATA packets
            ifs.seekg(0, std::ios::end);
            size_t length = ifs.tellg();
            size_t blocks = length / DATABLOCK;
            //add a block for last partial data (data less than datablock size)
            if (length % DATABLOCK > 0)  blocks++;
            std::cout << "Total blocks for " << filename  << ": " << blocks << std::endl;
            char dataBuff[DATABLOCK];
            memset(dataBuff, 0x00, DATABLOCK);
            uint16_t datablocksize = DATABLOCK;
            Data datablock{ htons(DATA), htons(transactionId)};
            ifs.seekg(0, ifs.beg);
            for (uint16_t i = 0; i < blocks; i++)
            {
                datablock.block_number = htons(i + 1);
                if (i >= blocks - 1) 
                {
                    datablocksize = (length % DATABLOCK > 0) ? length % DATABLOCK : DATABLOCK;
                }
                //memcpy(datablock.data, dataBuff, datablocksize);
                for (size_t j = 0; j < datablocksize; j++)
                {
                    datablock.data[j] = ifs.get();
                }

                uint16_t structSize = datablocksize + 6;  //2 bytes for message code, 2 bytes for transaction id, 2 bytes for block number
                Sleep(10);
                if (TRACE) std::cout << (i+1) << " ";

                int datasent = sendto(sock, (char*)&datablock, structSize, 0, (struct sockaddr*)&serverAddr,
                    sizeof(serverAddr));
                //std::cout << "Block: " << i + 1 << "  data sent: " << datasent << std::endl;
            }
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;
}

int main(int argc, char* argv[])
{
    char* serveraddr = getCmdOption(argv, argv + argc, "-serveraddr");
    char* serverport = getCmdOption(argv, argv + argc, "-serverport");
    char* port = getCmdOption(argv, argv + argc, "-port");
    char* timeoutinput = getCmdOption(argv, argv + argc, "-timeout");

    if (!serveraddr || !serverport || !port)
    {
        std::cout << "Incorrect statup options. Please enter:" << std::endl;
        std::cout << "-serveraddr [serveraddr] -serverport [serverport] -port [port]" << std::endl;
    }
    if (timeoutinput)
    {
        timeout = atoi(timeoutinput);
    }

    int sock;                        /* Socket */
    struct sockaddr_in servAddr;     /* Local address */
    struct sockaddr_in clientAddr;   /* Client address */
    //int cliLen;                      /* Length of incoming message */
    //int recvMsgSize;                 /* Size of received message */
    WSADATA wsaData;                 /* Structure for Windows Socket implementation */

    if (WSAStartup(MAKEWORD(2, 0), &wsaData) != 0) /* Load Winsock 2.0 DLL */
    {
        fprintf(stderr, "WSAStartup() failed");
        exit(1);
    }

    /* Create client socket for sending/receiving datagrams */
    if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        HandleError("socket() failed");
    /*if ((sock = WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, WSA_FLAG_OVERLAPPED)) < 0)
        HandleError("socket() failed");*/

    unsigned long nonblocking = 1;
    ioctlsocket(sock, FIONBIO, &nonblocking);
    //if (ioctlsocket(sock, FIONBIO, &nonblocking) != 0);

    struct timeval read_timeout;
    read_timeout.tv_sec = 0;
    read_timeout.tv_usec = 100000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&read_timeout, sizeof read_timeout);

    /*struct hostent* server;
    server = gethostbyname(serveraddr); /*returns pointer containing
    */

    /* Construct server address structure */
    memset(&servAddr, 0, sizeof(servAddr));        /* Reset structure */
    servAddr.sin_family = AF_INET;                 /* Internet address family */
    //InetPton(AF_INET, (PCWSTR)(serveraddr), &servAddr.sin_addr.s_addr);
    servAddr.sin_addr.s_addr = inet_addr(serveraddr);  /* Server IP address */
    servAddr.sin_port = htons(atoi(serverport));     /* Server port */

    /* Construct local address structure */
    memset(&clientAddr, 0, sizeof(clientAddr));     /* Reset structure */
    clientAddr.sin_family = AF_INET;                /* Internet address family */
    //InetPton(AF_INET, (PCWSTR)(htonl(INADDR_ANY)), &clientAddr.sin_addr.s_addr);
    clientAddr.sin_addr.s_addr = htonl(INADDR_ANY); /* Any incoming interface */
    clientAddr.sin_port = htons(atoi(port));      /* Local port */


    /* Bind to the local address */
    if (bind(sock, (struct sockaddr*)&clientAddr, sizeof(clientAddr)) < 0)
        HandleError("bind() failed");

    std::cout << "Client bound to port: " << port << std::endl;

    printClientHelp();

    for (;;) /* Run forever */
    {
        printf("$ ");
        std::string input;
        std::string part;
        std::getline(std::cin, input);
        std::stringstream ss(input);
        std::string commandargv[16];
        int commandargc = 0;
        while (std::getline(ss, part, ' ') && commandargc < 16) {
            commandargv[commandargc++] = part;
        }
        std::string s = commandargv[0];
        if (commandargv[0].compare("exit") == 0)
        {
            return 0;
        }
        if (commandargv[0].compare("help") == 0)
        {
            printClientHelpDetailed();
        }
        else if (commandargv[0].compare("listdir") == 0)
        {
            listDir(commandargv[1].c_str(), sock, servAddr);
        }
        else if (commandargv[0].compare("getfile") == 0)
        {
            std::vector files = split(commandargv[1].c_str(), ";");
            getFile(files, commandargv[2].c_str(), sock, servAddr);
        }
        else if (commandargv[0].compare("postfile") == 0)
        {
            std::vector files = split(commandargv[1].c_str(), ";");
            postFile(files, commandargv[2].c_str(), sock, servAddr);
        }
        else if (commandargv[0].compare("trace") == 0)
        {
            if (commandargv[1].compare("on") == 0) TRACE = true;
            if (commandargv[1].compare("off") == 0) TRACE = false;
            printf("Trace %s\n\n", TRACE ? "Enabled" : "Disabled");
        }
        else
        {
            std::cout << "command not recognized, call 'help' so find valid commands and formats" << std::endl;
        }
    }
    return 0;
}
