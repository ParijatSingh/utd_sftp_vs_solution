#include <stdio.h>    
#include <winsock.h>    /* for socket(),... */
#include <stdlib.h> 
#include <map>
#include "common_functions.h"
#include "packet.h"

#pragma comment(lib,"WS2_32")

//ROOT dir is passed during server startup. All the file upload will happen to root dir if no upload directory is specified
char* rootDir;
uint16_t transactionId = 100;
std::map<uint16_t, std::ofstream*> tx_Fd_Map;
bool TRACE = false;

/* External error handling function */
void HandleError(const char* errorMessage) {
    printf(errorMessage);
}

/* function to handle list directory request LISTDIRREQ. Generates the packet for each file or subdirectory.
   Packet Format:

     2 bytes    1 byte     2 bytes         0-256 bytes        1 byte
    ---------------------------------------------------------------
    | MSGCODE | DIR/FILE | FILESIZE(KB) | FILENAME             |\0|
    ---------------------------------------------------------------
    MSGCODE = 01; DIR/FILE = 'D' or 'F'
*/
void handleListRequest(int sock, char* buffer, sockaddr_in clientAddr)
{
    ListDirReq* req = reinterpret_cast<ListDirReq*>(buffer);
    // if no directory specified in request packet then list the root directory
    if (strlen(req->dirName) < 1) {
        strcpy_s(req->dirName, rootDir);
    }
    std::vector<fileInfo> data;
    try {
        data = getDirectoryListing(req->dirName);
        for (auto file = data.begin(); file != data.end(); ++file)
        {
            ListDirRes listdirinfo;
            listdirinfo.msgcode = htons(LISTDIRRES);
            listdirinfo.size = htons(ceil((file->size) / 1024.0));
            //if (listdirinfo.size == 0 && !file->dir) listdirinfo.size = htons(1);
            if (file->dir) listdirinfo.dir_or_file[0] = 'D';
            else listdirinfo.dir_or_file[0] = 'F';
            strcpy_s(listdirinfo.fileName, file->path.c_str());
            //int structSizeActual = sizeof(listDirRes);
            int structSize = sizeof(ListDirRes); ; // strlen(listdirinfo.fileName) + 6;

            if (sendto(sock, (char*)&listdirinfo, structSize, 0, (struct sockaddr*)&clientAddr,
                sizeof(clientAddr)) != structSize)
                HandleError("sendto() sent a different number of bytes than expected");
        }

    }
    catch (const std::exception& ex)
    {
        std::cout << ex.what() << '\n';
        Error error;
        error.msgcode = htons(ERR);
        strcpy_s(error.errormessage, ex.what());
        int msgsize = strlen(ex.what()) + 3;
        if (sendto(sock, (char*)&error, msgsize, 0, (struct sockaddr*)&clientAddr,
            sizeof(clientAddr)) != msgsize)
            HandleError("actual bytes sent does not match with intended bytes");
    }
}
/* function to handle file download request GETFILEREQ. Generates the packet for each file or subdirectory.
   Packet Format for GETFILERES:

     2 bytes    2 byte        0-256 bytes           1 byte
    ------------------------------------------------------
    | MSGCODE | TRANS ID | FILENAME                   |\0|
    ------------------------------------------------------
    MSGCODE = 04;
*/
void handleGetFileRequest(int sock, char* buffer, sockaddr_in clientAddr)
{
    GetFileReq* req = reinterpret_cast<GetFileReq*>(buffer);    
    std::ifstream ifs(req->fileName, std::ios::binary);
    if (ifs.is_open()) {
        // create new getfileresponse struct. add unique trancation id for each new file request
        GetFileRes getFileResponse = { htons(GETFILERES), htons(++transactionId) };
        strcpy_s(getFileResponse.fileName, req->fileName);
        int structlength = strlen(req->fileName) + 5; // 2 msgcode, 2 txId, 1 '\0'
        if (sendto(sock, (char*)&getFileResponse, structlength, 0, (struct sockaddr*)&clientAddr,
            sizeof(clientAddr)) != structlength)
            HandleError("actual bytes sent does not match with intended bytes");

        //after sending the GETFILERES, start sending FILEDATA packets
        ifs.seekg(0, std::ios::end);
        size_t length = ifs.tellg();
        int blocks = length / DATABLOCK;
        //add a block for last partial data (data less than datablock size)
        if (length % DATABLOCK > 0)  blocks++;

        printf("Sending %d blocks of %s  to %s:%d\n", blocks, req->fileName, inet_ntoa(clientAddr.sin_addr), htons(clientAddr.sin_port));

        char dataBuff[DATABLOCK];
        uint16_t datablocksize = DATABLOCK;
        ifs.seekg(0, ifs.beg);
        for (uint16_t i = 0; i < blocks; i++)
        {
            //ifs.seekg(256*i, ifs.beg);
            if (i < blocks - 1) {
                ifs.read(dataBuff, datablocksize);
            }
            else {
                datablocksize = (length % DATABLOCK > 0) ? length % DATABLOCK : DATABLOCK;
                ifs.read(dataBuff, datablocksize); //last block
            }
            Data datablock{ htons(DATA), htons(transactionId), htons(i + 1) };
            memcpy(datablock.data, dataBuff, datablocksize);
            if (TRACE) printf("%d[%d] ", (i + 1), transactionId);
            //std::cout << "Block DATA: " << datablock.data << std::endl;
            uint16_t structSize = datablocksize + 6;  //2 bytes for message code, 2 bytes for transaction id, 2 bytes for block number
            Sleep(5);
            if (sendto(sock, (char*)&datablock, structSize, 0, (struct sockaddr*)&clientAddr,
                sizeof(clientAddr)) != structSize)
                HandleError("actual bytes sent does not match with intended bytes");
            //std::cout << "block: " << (i + 1) << " sent" << std::endl;
        }
        //send an empty block in case of no partial block
        if (length % DATABLOCK == 0) {
            Data datablock{ htons(DATA), htons(transactionId), htons(0) };
            if (sendto(sock, (char*)&datablock, 6, 0, (struct sockaddr*)&clientAddr,
                sizeof(clientAddr)) != 6)
                HandleError("actual bytes sent does not match with intended bytes");
        }
        std::cout << std::endl;
    }
    else {
        // show message:
        char errBuff[DATABLOCK];
        strerror_s(errBuff, 100, errno);
        fprintf(stderr, errBuff);

        Error error;
        error.msgcode = htons(ERR);
        strcpy_s(error.errormessage, errBuff);
        int msgsize = strlen(errBuff) + 3;
        if (sendto(sock, (char*)&error, msgsize, 0, (struct sockaddr*)&clientAddr,
            sizeof(clientAddr)) != msgsize)
            HandleError("actual bytes sent does not match with intended bytes");
    }
}

/* function to handle file download request GETFILEREQ. Generates the packet for each file or subdirectory.
   Packet Format for GETFILERES:

     2 bytes    2 byte        1-MAXFILENAME bytes      0-MAXFILENAME bytes
    -----------------------------------------------------------------------
    | MSGCODE | TRANS ID | FILENAME                |  DIRPATH             |
    -----------------------------------------------------------------------
    MSGCODE = 04;
*/
void handlePostFileRequest(int sock, char* buffer, sockaddr_in clientAddr)
{
    PostFileReq* req = reinterpret_cast<PostFileReq*>(buffer);
    req->txId = ntohs(req->txId);
    char path[MAXFILENAME];
    if (strlen(req->dirPath) > 0) strcpy_s(path, req->dirPath);
    else strcpy_s(path, rootDir);
    strcat_s(path, "/");
    std::string filepath = path + extractFilename(req->fileName);

    tx_Fd_Map.insert(std::pair<uint16_t, std::ofstream*>(++transactionId, new std::ofstream(filepath, std::ios::binary | std::ios::out)));
    PostFileRes postFileResponse = { htons(POSTFILERES), htons(transactionId) };
    strcpy_s(postFileResponse.fileName, req->fileName);
    size_t structlength = strlen(req->fileName) + 5; // 2 msgcode, 2 txId, 1 - '\0'
    if (sendto(sock, (char*)&postFileResponse, structlength, 0, (struct sockaddr*)&clientAddr,
        sizeof(clientAddr)) != structlength)
        HandleError("actual bytes sent does not match with intended bytes");
}

/* function to handle the data packets sent by client when uploading file to server.
*  Locates the file pointer from the transaction ID and File map. The file os closed when the last block is received.
   Last black will be partial (<DATABLOCK size) or empty
   Packet Format for DATA:

     2 bytes    2 byte      2 byte           0-256 bytes
    ------------------------------------------------------------
    | MSGCODE | TRANS ID | BLOCK# |             DATA           |
    ------------------------------------------------------------
    MSGCODE = 04;
*/

void handlePostFileData(int sock, char* buffer, int recvMsgSize, sockaddr_in clientAddr)
{
    Data* datapkt = reinterpret_cast<Data*>(buffer);
    datapkt->txId = ntohs(datapkt->txId);
    datapkt->block_number = ntohs(datapkt->block_number);

    std::map<uint16_t, std::ofstream*>::iterator it = tx_Fd_Map.find(datapkt->txId);
    if (it != tx_Fd_Map.end())
    {
        //filestream found;
        if (recvMsgSize == -1) recvMsgSize = DATABLOCK + 6;
        writeFileBytes(*(it->second), datapkt->data, recvMsgSize - 6, DATABLOCK * (datapkt->block_number - 1));
        //if last blaock then close the filestream
        if (recvMsgSize < DATABLOCK + 6)
        {
            (*it->second).close();
        }
    }

    //send acknowledgement
    DataAck ack = { htons(ACK),htons(datapkt->txId),htons(datapkt->block_number) };
    sendto(sock, (char*)&ack, sizeof(ack), 0, (struct sockaddr*)&clientAddr, sizeof(clientAddr));
}

int main(int argc, char* argv[])
{
    char* port = getCmdOption(argv, argv + argc, "-port");
    rootDir = getCmdOption(argv, argv + argc, "-rootdir");
    if (getCmdOption(argv, argv + argc, "-trace"))
    {
        TRACE = true;
    }
    if (!port)
    {
        std::cout << "Incorrect statup options. Please enter:" << std::endl;
        std::cout << "-port [port] -rootdir [ROOT_FOLDER] -trace [on]{optional}" << std::endl;
        return 0;
    }
    if (!rootDir) rootDir = _strdup(".");

    int sock;                        /* Socket */
    struct sockaddr_in servAddr; /* Local address */
    struct sockaddr_in clientAddr; /* Client address */
    char msgBuffer[MSGMAXSIZE];        /* Buffer for message*/
    unsigned short serverPort;     /* Server port */
    int cliLen;                      /* Length of incoming message */
    int recvMsgSize;                 /* Size of received message */
    WSADATA wsaData;                 /* Structure for WinSock setup communication */

    serverPort = atoi(port);

    if (WSAStartup(MAKEWORD(2, 0), &wsaData) != 0) /* Load Winsock 2.0 DLL */
    {
        fprintf(stderr, "WSAStartup() failed");
        exit(1);
    }

    /* Create socket for sending/receiving datagrams */
    if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        HandleError("socket() failed");

    /* Construct local address structure */
    memset(&servAddr, 0, sizeof(servAddr));   /* Zero out structure */
    servAddr.sin_family = AF_INET;                /* Internet address family */
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY); /* Any incoming interface */
    servAddr.sin_port = htons(serverPort);      /* Local port */

    /* Bind to the local address */
    if (bind(sock, (struct sockaddr*)&servAddr, sizeof(servAddr)) < 0)
        HandleError("bind() failed");

    std::cout << "Server listening for messages. Bound to port: " << serverPort << std::endl << std::endl;

    for (;;) /* Run forever */
    {
        /* Set the size of the in-out parameter */
        cliLen = sizeof(clientAddr);

        /* Block until receive message from a client */
        if ((recvMsgSize = recvfrom(sock, msgBuffer, MSGMAXSIZE, 0,
            (struct sockaddr*)&clientAddr, &cliLen)) < 0)
        {
            HandleError("recvfrom() failed");
            //continue;
        }
        //printf("Handling client Port %d\n", );

        Packet* pkt = reinterpret_cast<Packet*>(msgBuffer);
        uint16_t msgCode = ntohs(pkt->msgcode);

        if (msgCode < DATA)
        {
            printf("%s message from client %s:%d\n", getMsgName(msgCode).c_str(), inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));
        }

        switch (msgCode) {
        case LISTDIRREQ:
            handleListRequest(sock, msgBuffer, clientAddr);
            break;
        case GETFILEREQ:
            handleGetFileRequest(sock, msgBuffer, clientAddr);
            break;
        case POSTFILEREQ:
            handlePostFileRequest(sock, msgBuffer, clientAddr);
            break;
        case DATA:
            handlePostFileData(sock, msgBuffer, recvMsgSize, clientAddr);
            break;
        case ACK:
            //DO NOTHING for NOW
            break;
        default:
            printf("Message Code Not Found");
        }

        printf(msgBuffer);

        /*if (sendto(sock, echoBuffer, recvMsgSize, 0, (struct sockaddr*)&echoClntAddr,
            sizeof(echoClntAddr)) != recvMsgSize)
            HandleError("actual bytes sent does not match with intended bytes");*/

    }
    return 0;
}