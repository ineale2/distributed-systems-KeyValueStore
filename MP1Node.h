/**********************************
 * FILE NAME: MP1Node.cpp
 *
 * DESCRIPTION: Membership protocol run by this Node.
 * 				Header file of MP1Node class.
 **********************************/

#ifndef _MP1NODE_H_
#define _MP1NODE_H_

#include "stdincludes.h"
#include "Log.h"
#include "Params.h"
#include "Member.h"
#include "EmulNet.h"
#include "Queue.h"

/**
 * Macros
 */
#define TREMOVE 20
#define TFAIL 5
#define DB_TIMEOUT 15


// Message sizes
#define SMALL_MSG_SIZE sizeof(msg)
#define PING_MSG_SIZE SMALL_MSG_SIZE
#define ACK_MSG_SIZE  SMALL_MSG_SIZE

// Tuning constants
#define M 5 //Number of processes to randomly ping
#define K 1 //Number of processes to select for indirect ping

#define DELTA_BUFF_SIZE 30
/*
 * Note: You can change/add any functions in MP1Node.{h,cpp}
 */

/**
 * Message Types
 */
enum msgTypes{
    JOINREQ,
    JOINREP,
    PING,
    ACK 
};

enum dbTypes{
	FAILED,
	JOINED,
	REJUV,
	EMPTY 
};

enum nodeStatus{
	NOT_SUSPECTED,
	SUSPECTED
};

enum pingStatus{
	NOT_PINGED,
	PINGED 
};
/**
 * STRUCT NAME: msg
 *
 * DESCRIPTION: Header and content of a message
 */
typedef struct msg{
	msgTypes msgType;
	Address sender;
	dbTypes gossipType;	
	Address dbAddr;
	msg(msgTypes mt, Address sen, dbTypes gt, Address dba) : msgType(mt), sender(sen), gossipType(gt), dbAddr(dba)
	{
	}
}msg;

struct nodeData{
	enum nodeStatus nstat;
	enum pingStatus pstat;
	long pseq;  //Sequence number for ping queue
	long dbseq; //Sequence number for deltaBuffer
	long sseq;  //Sequence number for suspects queue
	nodeData(nodeStatus ns, pingStatus ps, long psq, long ds, long ss): nstat(ns), pstat(ps), pseq(psq), dbseq(ds), sseq(ss) 
	{
	}
	nodeData() : nstat(NOT_SUSPECTED), pstat(NOT_PINGED), pseq(0), dbseq(0), sseq(0)
	{
	}
};

struct pingData{
	long expTime;
	long pseq;
	string addr;
	pingData(long et, long s, string a) : expTime(et), pseq(s), addr(a){
	}
	pingData() : expTime(0), pseq(0), addr()
	{
	}
};

struct dbData{
	string addr;
	dbTypes dbType;
	long dbseq;
	dbData(string a, dbTypes d, long s) : addr(a), dbType(d), dbseq(s){
	}
	dbData() : addr(), dbType(EMPTY), dbseq(0){
	}
};

/**
 * CLASS NAME: MP1Node
 *
 * DESCRIPTION: Class implementing Membership protocol functionalities for failure detection
 */

class MP1Node {
private:
	/* Private Data */
	EmulNet *emulNet;
	Log *log;
	Params *par;
	Member *memberNode;
	
	/* pinged is a queue of the processes that this node has pinged */
	queue<pingData> pinged;

	/* memberMap is a map of all nodes known to this node from the address (as a string) to a struct of status for the node */
	map<string, nodeData> memberMap; 

	/* suspects is a queue of all nodes that this node suspects to be failed. 
	The pair is the address of the process and the time at which the suspected process will be removed from the member map */
	deque<pair<string, long   > > suspects;

	/* deltaBuff is a queue of events to gossip about. dbit is an iterator over the deltaBuff */
	deque<dbData> deltaBuff;	
	deque<dbData>::iterator dbit;
	int dbTimer;

	char NULLADDR[6];

	/* Private Methods */
	Address readDeltaBuff(dbTypes*);
	void 	writeDeltaBuff(Address, dbTypes);
	char* 	createJOINREP(size_t* msgSize);
	msg* 	createMessage(msgTypes msgType);
	Address processJOINREQ(char* mIn);
	void 	processJOINREP(char* mIn, int size);
	Address processMessage(msg* mIn);
    void	addMember(Address addr);	
	void 	removeMember(Address addr);

public:
	MP1Node(Member *, Params *, EmulNet *, Log *, Address *);
	Member * getMemberNode() {
		return memberNode;
	}
	int recvLoop();
	static int enqueueWrapper(void *env, char *buff, int size);
	void nodeStart(char *servaddrstr, short serverport);
	int initThisNode(Address *joinaddr);
	int introduceSelfToGroup(Address *joinAddress);
	int finishUpThisNode();
	void nodeLoop();
	void checkMessages();
	bool recvCallBack(void *env, char *data, int size);
	void initMemberListTable(Member *memberNode);
	void nodeLoopOps();
	int isNullAddress(Address *addr);
	Address getJoinAddress();
	void printAddress(Address *addr);
	void decomposeAddr(string addr, long* id, long* port);
	virtual ~MP1Node();
};

#endif /* _MP1NODE_H_ */
