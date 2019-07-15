/**********************************
 * FILE NAME: MP2Node.h
 *
 * DESCRIPTION: MP2Node class header file
 **********************************/

#ifndef MP2NODE_H_
#define MP2NODE_H_

/**
 * Header files
 */
#include "stdincludes.h"
#include "EmulNet.h"
#include "Node.h"
#include "HashTable.h"
#include "Log.h"
#include "Params.h"
#include "Message.h"
#include "Queue.h"
#include <unordered_map>

#define NUM_REPLICAS 3
#define T_CLOSE 25

class Transaction{
private:
	int				id;    		// Transaction ID
	string 			key;   		// Key for this transaction
	string 			value;		// Value for this transaction
	vector<string> 	replys;		// Replys to transaction
	MessageType 	type;		// Transaction type
	int 			stime; 		// Start time of transaction
	Log*			log;		// Place to log this transaction
	int				numOK;		// Number of OK replys
	int				numReplys;  // Number of replys

public:
	Transaction(int i, string k, MessageType ty, int st, Log* l);
	Transaction(int i, string k, string v, MessageType ty, int st, Log* l);
	Transaction();

	int 	 addReply(string reply);
	int  	addReply(bool status);
	string 	close(bool*);
	int		getID();
	int  	getStartTime();
	string 	getKey();
	string 	getValue();
	MessageType getType();
};

/**
 * CLASS NAME: MP2Node
 *
 * DESCRIPTION: This class encapsulates all the key-value store functionality
 * 				including:
 * 				1) Ring
 * 				2) Stabilization Protocol
 * 				3) Server side CRUD APIs
 * 				4) Client side CRUD APIs
 */
class MP2Node {
private:
	// Vector holding the next two neighbors in the ring who have my replicas
	vector<Node> hasMyReplicas;
	// Vector holding the previous two neighbors in the ring whose replicas I have
	vector<Node> haveReplicasOf;
	// Ring
	vector<Node> ring;
	// Hash Table
	HashTable * ht;
	// Member representing this member
	Member *memberNode;
	// Params object
	Params *par;
	// Object of EmulNet
	EmulNet * emulNet;
	// Object of Log
	Log * log;
	// Transactions that are currently open at this node 
	unordered_map<int, Transaction> tmap;

public:
	MP2Node(Member *memberNode, Params *par, EmulNet *emulNet, Log *log, Address *addressOfMember);
	Member * getMemberNode() {
		return this->memberNode;
	}

	// ring functionalities
	void updateRing();
	vector<Node> getMembershipList();
	size_t hashFunction(string key);
	void findNeighbors();

	// client side CRUD APIs
	void clientCreate(string key, string value);
	void clientRead(string key);
	void clientUpdate(string key, string value);
	void clientDelete(string key);

	// receive messages from Emulnet
	bool recvLoop();
	static int enqueueWrapper(void *env, char *buff, int size);

	void sendMessage(Address *toAddr, Message* msg);
	void sendMsgToReplicas(string* key, Message* msg);

	void sendREPLY(int* transID, Address* sender, MessageType type, bool status);
	// handle messages from receiving queue
	void checkMessages();

	// coordinator dispatches messages to corresponding nodes
	void dispatchMessages(Message message);

	// find the addresses of nodes that are responsible for a key
	vector<Node> findNodes(string key);

	// server
	bool createKeyValue(string key, string value, ReplicaType replica);
	string readKey(string key);
	bool updateKeyValue(string key, string value, ReplicaType replica);
	bool deletekey(string key);

	// stabilization protocol - handle multiple failures
	void stabilizationProtocol();

	// logging methods
	void logAction(MessageType type, int tid, bool isCoord, string key, string value, bool status);
	~MP2Node();
};

#endif /* MP2NODE_H_ */
