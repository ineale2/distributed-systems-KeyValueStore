/**********************************
TODO: Consider rewriting to make strings dynamic and prevent string copying
TODO: Make logaction CONST
 * FILE NAME: MP2Node.cpp
 *
 * DESCRIPTION: MP2Node class definition
 **********************************/
#include "MP2Node.h"

/**
 * constructor
 */
MP2Node::MP2Node(Member *memberNode, Params *par, EmulNet * emulNet, Log * log, Address * address) {
	this->memberNode = memberNode;
	this->par = par;
	this->emulNet = emulNet;
	this->log = log;
	ht = new HashTable();
	this->memberNode->addr = *address;
}

/**
 * Destructor
 */
MP2Node::~MP2Node() {
	delete ht;
	delete memberNode;
}

/**
 * FUNCTION NAME: updateRing
 *
 * DESCRIPTION: This function does the following:
 * 				1) Gets the current membership list from the Membership Protocol (MP1Node)
 * 				   The membership list is returned as a vector of Nodes. See Node class in Node.h
 * 				2) Constructs the ring based on the membership list
 * 				3) Calls the Stabilization Protocol
 */
void MP2Node::updateRing() {
	/*
	 * Implement this. Parts of it are already implemented
	 */
	static long last_seq = -1;
	// Compare last_seq to heartbeat to determine if there was a change
	if(last_seq == memberNode->heartbeat){
		return;
	}
	// Store sequence number for next time
	last_seq = memberNode->heartbeat;

	// Get the current membership list from Membership Protocol / MP1
	ring = getMembershipList();

	// Sort the list based on the hashCode
	sort(ring.begin(), ring.end());

	// Run stabilization protocol if the hash table size is greater than zero and if there has been a changed in the ring
	if(!ht->isEmpty()){
		stabilizationProtocol();
	}
}

/**
 * FUNCTION NAME: getMemberhipList
 *
 * DESCRIPTION: This function goes through the membership list from the Membership protocol/MP1 and
 * 				i) generates the hash code for each member
 * 				ii) populates the ring member in MP2Node class
 * 				It returns a vector of Nodes. Each element in the vector contain the following fields:
 * 				a) Address of the node
 * 				b) Hash code obtained by consistent hashing of the Address
 */
vector<Node> MP2Node::getMembershipList() {
	unsigned int i;
	vector<Node> curMemList;
	for ( i = 0 ; i < this->memberNode->memberList.size(); i++ ) {
		Address addressOfThisMember;
		int id = this->memberNode->memberList.at(i).getid();
		short port = this->memberNode->memberList.at(i).getport();
		memcpy(&addressOfThisMember.addr[0], &id, sizeof(int));
		memcpy(&addressOfThisMember.addr[4], &port, sizeof(short));
		curMemList.emplace_back(Node(addressOfThisMember));
	}
	return curMemList;
}

/**
 * FUNCTION NAME: hashFunction
 *
 * DESCRIPTION: This functions hashes the key and returns the position on the ring
 * 				HASH FUNCTION USED FOR CONSISTENT HASHING
 *
 * RETURNS:
 * size_t position on the ring
 */
size_t MP2Node::hashFunction(string key) {
	std::hash<string> hashFunc;
	size_t ret = hashFunc(key);
	return ret%RING_SIZE;
}

/**
 * FUNCTION NAME: clientCreate
 *
 * DESCRIPTION: client side CREATE API
 * 				The function does the following:
 * 				1) Constructs the message
 * 				2) Finds the replicas of this key
 * 				3) Sends a message to the replica
 */
// Note: This method is poorly named, but this name is required for grading
// This method is executed by the coordinator node
void MP2Node::clientCreate(string key, string value) {
	int tid = g_transID++;
	// Generate new transaction ID and a new message for this request
	Message msg(tid, memberNode->addr, CREATE, key, value); 

	// Send to replicas that have this key
	sendMsgToReplicas(&key, &msg); 

	// Open a new transaction
	tmap[tid] = Transaction(tid, key, value, CREATE, par->getcurrtime(), log); 	
	
}

/**
 * FUNCTION NAME: clientRead
 *
 * DESCRIPTION: client side READ API
 * 				The function does the following:
 * 				1) Constructs the message
 * 				2) Finds the replicas of this key
 * 				3) Sends a message to the replica
 */
void MP2Node::clientRead(string key){
	int tid= g_transID++;
	// Generate new transaction ID and a new message for this request
	Message msg(tid, memberNode->addr, READ, key); 

	// Send to replicas that have this key
	sendMsgToReplicas(&key, &msg); 

	// Open a new transaction
	tmap[tid] = Transaction(tid, key, READ, par->getcurrtime(), log); 	
}

/**
 * FUNCTION NAME: clientUpdate
 *
 * DESCRIPTION: client side UPDATE API
 * 				The function does the following:
 * 				1) Constructs the message
 * 				2) Finds the replicas of this key
 * 				3) Sends a message to the replica
 */
void MP2Node::clientUpdate(string key, string value){
	int tid = g_transID++;
	// Generate new transaction ID and a new message for this request
	Message msg(tid, memberNode->addr, UPDATE, key, value); 

	// Send to replicas that have this key
	sendMsgToReplicas(&key, &msg); 

	// Open a new transaction
	tmap[tid] = Transaction(tid, key, value, UPDATE, par->getcurrtime(), log); 	
}

/**
 * FUNCTION NAME: clientDelete
 *
 * DESCRIPTION: client side DELETE API
 * 				The function does the following:
 * 				1) Constructs the message
 * 				2) Finds the replicas of this key
 * 				3) Sends a message to the replica
 */
void MP2Node::clientDelete(string key){
	int tid = g_transID++;
	// Generate new transaction ID and a new message for this request
	Message msg(tid, memberNode->addr, DELETE, key); 

	// Send to replicas that have this key
	sendMsgToReplicas(&key, &msg); 

	// Open a new transaction
	tmap[tid] = Transaction(tid, key, DELETE, par->getcurrtime(), log); 	
}

/**
 * FUNCTION NAME: createKeyValue
 *
 * DESCRIPTION: Server side CREATE API
 * 			   	The function does the following:
 * 			   	1) Inserts key value into the local hash table
 * 			   	2) Return true or false based on success or failure
 */
bool MP2Node::createKeyValue(string key, string value, ReplicaType replica) {
	Entry v(value, par->getcurrtime(), replica);
	return ht->create(key, v.convertToString()); 
}

/**
 * FUNCTION NAME: readKey
 *
 * DESCRIPTION: Server side READ API
 * 			    This function does the following:
 * 			    1) Read key from local hash table
 * 			    2) Return value
 */
string MP2Node::readKey(string key) {
	return ht->read(key);
}

/**
 * FUNCTION NAME: updateKeyValue
 *
 * DESCRIPTION: Server side UPDATE API
 * 				This function does the following:
 * 				1) Update the key to the new value in the local hash table
 * 				2) Return true or false based on success or failure
 */
bool MP2Node::updateKeyValue(string key, string value, ReplicaType replica) {
	Entry v(value, par->getcurrtime(), replica);
	return ht->update(key, v.convertToString()); 
}

/**
 * FUNCTION NAME: deleteKey
 *
 * DESCRIPTION: Server side DELETE API
 * 				This function does the following:
 * 				1) Delete the key from the local hash table
 * 				2) Return true or false based on success or failure
 */
bool MP2Node::deletekey(string key) {
	return ht->deleteKey(key);
}

/**
 * FUNCTION NAME: checkMessages
 *
 * DESCRIPTION: This function is the message handler of this node.
 * 				This function does the following:
 * 				1) Pops messages from the queue
 * 				2) Handles the messages according to message types
 */
void MP2Node::checkMessages() {
	/*
	 * Implement this. Parts of it are already implemented
	 */
	char * data;
	int size;

	/*
	 * Declare your local variables here
	 */
	bool status;	// Transaction success
	int numReplys; 	// Number of replys so far                         
	string val;		// Value returned by read (value::timestamp::replicaType)

	// dequeue all messages and handle them
	while ( !memberNode->mp2q.empty() ) {
		/*
		 * Pop a message from the queue
		 */
		data = (char *)memberNode->mp2q.front().elt;
		size = memberNode->mp2q.front().size;
		memberNode->mp2q.pop();

		string message(data, data + size);

		Message msg(message);
		switch(msg.type){
			case CREATE:
			{
				// Attempt to create the KV
				status = createKeyValue(msg.key, msg.value, msg.replica);

				// Reply to message
				sendREPLY(&msg.transID, &memberNode->addr, msg.type, status); 

				logAction(CREATE, msg.transID, false, msg.key, msg.value, status);
				break;
			}
			case READ:  
			{
				val  = readKey(msg.key);
				// Check for empty string for success
				status = !val.empty();

				// Create a READREPLY message
				Message rred(msg.transID, memberNode->addr, READREPLY, val);
				// Send READREPLY message to sender of received message
				sendMessage(&msg.fromAddr, &rred);

				// Use entry constructor to remove timestamp and type info
				Entry e(val);
				logAction(READ, msg.transID, false, msg.key, e.value, status);
				break;
			}
			case UPDATE:
			{
				// Make the update
				status = updateKeyValue(msg.key, msg.value, msg.replica);
				// Reply to message
				sendREPLY(&msg.transID, &memberNode->addr, msg.type, status); 

				logAction(UPDATE, msg.transID, false, msg.key, msg.value, status);
				break;
			}
			case DELETE:
			{
				// Delete the key
				status = deletekey(msg.key);

				// Reply to message
				sendREPLY(&msg.transID, &memberNode->addr, msg.type, status); 

				logAction(DELETE, msg.transID, false, msg.key, msg.value, status);
				break;
			}
			case REPLY: 
			{
				// Record reply in transaction
				auto it = tmap.find(msg.transID);
				if(it != tmap.end()){
					numReplys = it->second.addReply(msg.success);
					// If all replys have been recieved, then close the transaction and then delete it.
					if(numReplys == NUM_REPLICAS){
						val = it->second.close(&status);
						logAction(msg.type, msg.transID, true, it->second.getKey(), it->second.getValue(), status); 
						tmap.erase(msg.transID);
					}
				}
				else{
					cout << "TRANSACTION NOT RECOGNIZED" << endl;
				}
				// No reply needed
				break;
			}
			case READREPLY:
			{
				// Record reply in transaction
				auto it = tmap.find(msg.transID);
				if(it != tmap.end()){
					numReplys = it->second.addReply(msg.value);
					// If all replys have been recieved, then close the transaction and then delete it.
					if(numReplys == NUM_REPLICAS){
						val = it->second.close(&status);
						logAction(msg.type, msg.transID, true, it->second.getKey(), val, status); 
						tmap.erase(msg.transID);
					}
				}
				else{
					cout << "TRANSACTION NOT RECOGNIZED" << endl;
				}
				// Logging is handled by the transaction class
				// No reply needed
				break;
			}
		}

	}

	/*
	 * This function should also ensure all READ and UPDATE operation
	 * get QUORUM replies
	 */
	// TODO: Should you ping if you don't get a response? 
	int currtime = par->getcurrtime();
	for(auto it = tmap.begin(); it != tmap.end(); it++){
		if(it->second.getStartTime() + T_CLOSE < currtime){
			val = it->second.close(&status);
			logAction(it->second.getType(), it->second.getID(), true, it->second.getKey(), val, status);
			tmap.erase(it);
		}
	}
}

void MP2Node::sendREPLY(int* transID, Address* sender, MessageType type, bool status){
	Message rep(*transID, memberNode->addr, type, status); 
	sendMessage(sender, &rep);
}

void MP2Node::logAction(MessageType type, int tid, bool isCoord, string key, string value, bool status){
	// if READ, then make sure to digest the key into non-delimited fashion		
	switch(type){
		case CREATE:
		{
			if(status)
				log->logCreateSuccess(&memberNode->addr, isCoord, tid, key, value);
			else
				log->logCreateFail(	 &memberNode->addr, isCoord, tid, key, value);
			break;
		}

		case READ:  
		{
			if(status)
				log->logReadSuccess(&memberNode->addr, isCoord, tid, key, value);
			else
				log->logReadFail(   &memberNode->addr, isCoord, tid, key);
			break;
		}

		case UPDATE:
		{
			if(status)
				log->logUpdateSuccess(&memberNode->addr, isCoord, tid, key, value);
			else
				log->logUpdateFail(   &memberNode->addr, isCoord, tid, key, value);
			break;
		}

		case DELETE:
		{
			if(status)	 
				log->logDeleteSuccess(&memberNode->addr, isCoord, tid, key);	
			else
				log->logDeleteFail(   &memberNode->addr, isCoord, tid, key);
			break;
		}
		default:{
			cout << "ERROR IN LOGACTION" << endl;
		}
	}

}
/**
 * FUNCTION NAME: findNodes
 *
 * DESCRIPTION: Find the replicas of the given keyfunction
 * 				This function is responsible for finding the replicas of a key
 */
vector<Node> MP2Node::findNodes(string key) {
	size_t pos = hashFunction(key);
	vector<Node> addr_vec;
	if (ring.size() >= 3) {
		// if pos <= min || pos > max, the leader is the min
		if (pos <= ring.at(0).getHashCode() || pos > ring.at(ring.size()-1).getHashCode()) {
			addr_vec.emplace_back(ring.at(0));
			addr_vec.emplace_back(ring.at(1));
			addr_vec.emplace_back(ring.at(2));
		}
		else {
			// go through the ring until pos <= node
			for (int i=1; i<ring.size(); i++){
				Node addr = ring.at(i);
				if (pos <= addr.getHashCode()) {
					addr_vec.emplace_back(addr);
					addr_vec.emplace_back(ring.at((i+1)%ring.size()));
					addr_vec.emplace_back(ring.at((i+2)%ring.size()));
					break;
				}
			}
		}
	}
	return addr_vec;
}

/**
 * FUNCTION NAME: recvLoop
 *
 * DESCRIPTION: Receive messages from EmulNet and push into the queue (mp2q)
 */
bool MP2Node::recvLoop() {
    if ( memberNode->bFailed ) {
    	return false;
    }
    else {
    	return emulNet->ENrecv(&(memberNode->addr), this->enqueueWrapper, NULL, 1, &(memberNode->mp2q));
    }
}

/**
 * FUNCTION NAME: enqueueWrapper
 *
 * DESCRIPTION: Enqueue the message from Emulnet into the queue of MP2Node
 */
int MP2Node::enqueueWrapper(void *env, char *buff, int size) {
	Queue q;
	return q.enqueue((queue<q_elt> *)env, (void *)buff, size);
}
/**
 * FUNCTION NAME: stabilizationProtocol
 *
 * DESCRIPTION: This runs the stabilization protocol in case of Node joins and leaves
 * 				It ensures that there always 3 copies of all keys in the DHT at all times
 * 				The function does the following:
 *				1) Ensures that there are three "CORRECT" replicas of all the keys in spite of failures and joins
 *				Note:- "CORRECT" replicas implies that every key is replicated in its two neighboring nodes in the ring
 */
void MP2Node::stabilizationProtocol() {
	/*
	 * Implement this
	 */
	// Need to update hasMyReplicas every time
	// Need to determine if the most recent change effected one of your replicas (check for hasMyReplicas in membership list (look at position in ring as well as address) 
	// Send a message to write all of your values (make sure not to copy key type) 
	// Don't over replicate keys
	// make one type of node responsible for replicating
	// primary replicates secondary or tertiary, but secondary replicates primary, tertiary replicates none
}

void MP2Node::sendMessage(Address *toAddr, Message* msg){
	emulNet->ENsend(&memberNode->addr, toAddr, msg->toString());
}

void MP2Node::sendMsgToReplicas(string* key, Message* msg){
	// Get vector of nodes where this key is stored
	vector<Node> nodes = findNodes(*key);
	
	// Send the message to them
	for(int i = 0; i < nodes.size(); i++){
		sendMessage(&nodes[i].nodeAddress, msg);	
	}
}

/* START TRANSATION CLASS */
Transaction::Transaction() : id(0), key(""), type(READ), stime(0), log(NULL), numOK(0), numReplys(0){
}

Transaction::Transaction(int i, string k, MessageType ty, int st, Log* l) : id(i), key(k), type(ty), stime(st), log(l), numOK(0), numReplys(0){
		if(ty == READ){
			replys.reserve(NUM_REPLICAS);
		}
}

Transaction::Transaction(int i, string k, string v, MessageType ty, int st, Log* l) : id(i), key(k), value(v), type(ty), stime(st), log(l), numOK(0), numReplys(0){
		if(ty == READ){
			replys.reserve(NUM_REPLICAS);
		}
}
	// Returns a boolean if this transaction was closed
int Transaction::addReply(string reply){
		numReplys++;
		replys.push_back(reply);	
		if(!reply.empty()){
			numOK++;
		}
		return numReplys;
		
}

int Transaction::addReply(bool transOK){
		// Record reply
		numReplys++;
		if(transOK){
			numOK++;
		}
		return numReplys;
}

string Transaction::close(bool* st){
		string rep;
		if(type != READ){
			*st = (numOK > (int)(NUM_REPLICAS/2));
			return ""; 
		}
		else{
			// Need to check for quorum 
			// qmap is a map to check quorum, will map each value to number of times it has been seen in replys
			unordered_map<string, int> :: iterator it;
			unordered_map<string, int> qmap; 
			while(!replys.empty()){
				Entry en(replys.back());
				replys.pop_back();
				it = qmap.find(en.value);
				// Insert into map
				if(it == qmap.end()){
					qmap[en.value] = 1;	
				}
				else{
					qmap[en.value]++;
				}
			}
			
			// Iterate over unordered map to check for quorum
			for(it = qmap.begin(); it !=qmap.end(); it++){
				if(it->second > (int)(NUM_REPLICAS/2)){
					*st = true;
					return it->first;
				}	
			}
			*st = false;
			return "";
		}
		
}


int Transaction::getStartTime(){
		return stime;
}	

string Transaction::getKey(){
	return key;
}
string Transaction::getValue(){
	return value;
}
MessageType Transaction::getType(){
	return type;
}
int Transaction::getID(){
	return id;
}
