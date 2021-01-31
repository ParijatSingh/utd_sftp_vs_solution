#define MAXFILENAME 128
#define DATABLOCK 512
#define MSGMAXSIZE DATABLOCK+6

enum messagecode {
	LISTDIRREQ  = 1,
	LISTDIRRES  = 2,
	GETFILEREQ  = 3,
	GETFILERES  = 4,
	POSTFILEREQ = 5,
	POSTFILERES = 6,
	DATA        = 7,
	ERR         = 8,
	ACK         = 9
};

struct Packet {
	uint16_t msgcode;
	char data[MAXFILENAME];
};

struct ListDirReq {
	uint16_t msgcode; /* LISTFILEREQ */
	char dirName[MAXFILENAME];
};

struct ListDirRes {
	uint16_t msgcode; /* LISTFILERES */
	uint16_t size;
	char dir_or_file[1];
	char fileName[MAXFILENAME];
};

struct GetFileReq {
	uint16_t msgcode; /* GETFILEREQ */
	char fileName[MAXFILENAME];
};

struct GetFileRes {
	uint16_t msgcode; /* LISTFILERES */
	uint16_t txId;
	char fileName[MAXFILENAME];
};

struct PostFileReq {
	uint16_t msgcode; /* LISTFILERES */
	uint16_t txId;
	char fileName[MAXFILENAME];
	char dirPath[MAXFILENAME];
};

struct PostFileRes {
	uint16_t msgcode; /* LISTFILERES */
	uint16_t txId;
	char fileName[MAXFILENAME];
};

struct Data {
	uint16_t msgcode; /* DATA */
	uint16_t txId;
	uint16_t block_number;
	uint8_t data[DATABLOCK];
};

struct DataAck {
	uint16_t msgcode; /* ACK */
	uint16_t txId;
	uint16_t block_number;
};

struct Error {
	uint16_t msgcode; /* ERR */
	char errormessage[DATABLOCK];
};
