/**********************************
TODO: memory check in valgrind
TODO: make findNodes return a set
TODO: Consider rewriting to make strings dynamic and prevent string copying
TODO: Make logaction CONST
TODO: Figure out how to prevent over replication
TODO: figure out why reads return such long values
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
	this->last_seq = INIT_SEQ;
	this->hasMyReplicas = findNodes(address->getAddress());
}

/**
 * Destructor
 */
MP2Node::~MP2Node() {
	delete ht;
	//delete memberNode;
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

void MP2Node::printRing(){
	cout << par->getcurrtime() << " " << memberNode->addr.getAddress() <<  " RING" << endl;
	for(auto it = ring.begin(); it != ring.end(); it++){
		cout << it->getAddress()->getAddress() << " "; 
	}
	cout << endl;

}
void MP2Node::updateRing() {
	/*
	 * Implement this. Parts of it are already implemented
	 */
	// Compare last_seq to heartbeat to determine if there was a change
	if(last_seq == memberNode->heartbeat){
		return;
	}
	
	// Store sequence number for next time
	last_seq = memberNode->heartbeat;
	
	cout << "OLD RING" << endl;
	printRing();
	
	ring = getMembershipList();

	// Sort the list based on the hashCode
	sort(ring.begin(), ring.end());

	// Print membership list
	
	cout << "NEW RING" << endl;
	printRing();
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
	sendCreateToReplicas(&key, &msg); 

	// Open a new transaction
	tmap[tid] = Transaction(tid, key, value, CREATE, par->getcurrtime(), log); 	
	
}

void MP2Node::sendCreateToReplicas(string* key, Message* msg){
	// In order to generalize for NUM_REPLICAS != 3, need to iterate over enum
	// Need to define ++ operator for ReplicaType, but this code cannot be changed per the grader instructions
	// Also note, findNodes, which was provided, does not generalize with NUM_REPLIAS !=3
	vector<Node> nodes = findNodes(*key);
	msg->replica = PRIMARY;
	sendMessage(&nodes[0].nodeAddress, msg);

	msg->replica = SECONDARY;
	sendMessage(&nodes[1].nodeAddress, msg);

	msg->replica = TERTIARY;
	sendMessage(&nodes[2].nodeAddress, msg);
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
		if(msg.transID == STABILIZATION_TID){
			cout << par->getcurrtime() << " " << memberNode->addr.getAddress() << " got STAB CREATE for key " << msg.key << " from " << msg.fromAddr.getAddress() << endl;
		}
		switch(msg.type){
			case CREATE:
			{
				// Attempt to create the KV
				status = createKeyValue(msg.key, msg.value, msg.replica);

				// Reply to message
				sendREPLY(msg.transID, &msg.fromAddr, status); 

				logAction(CREATE, msg.transID, false, msg.key, msg.value, status);
				break;
			}
			case READ:  
			{
				val  = readKey(msg.key);
				// Check for empty string for success
				status = !val.empty();

				// Only reply if read was succesful
				if(status){
					// Create a READREPLY message
					Message rred(msg.transID, memberNode->addr, val);
					// Send READREPLY message to sender of received message
					sendMessage(&msg.fromAddr, &rred);
					// Use entry constructor to remove timestamp and type info
					cout << par->getcurrtime() << " " << memberNode->addr.getAddress() << " readKey("<<msg.key<<") = " << val << endl;
					Entry e(val);
					val = e.value;
				}
				logAction(READ, msg.transID, false, msg.key, val, status);
				break;
			}
			case UPDATE:
			{
				// Make the update
				status = updateKeyValue(msg.key, msg.value, msg.replica);
				// Reply to message
				sendREPLY(msg.transID, &msg.fromAddr, status); 

				logAction(UPDATE, msg.transID, false, msg.key, msg.value, status);
				break;
			}
			case DELETE:
			{
				// Delete the key
				status = deletekey(msg.key);

				// Reply to message
				sendREPLY(msg.transID, &msg.fromAddr, status); 

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
						cout << par->getcurrtime() << " " << memberNode->addr.getAddress() << " got all replys for transID = " << msg.transID << endl;
						val = it->second.close(&status);
						logAction(it->second.getType(), it->second.getID(), true, it->second.getKey(), it->second.getValue(), status); 
						tmap.erase(it);
					}
				}
				else{
					cout << "transID = " << msg.transID  << endl;
					cout << "TRANSACTION NOT RECOGNIZED" << endl;
				}
				// No reply needed
				break;
			}
			case READREPLY:
			{
				// Record reply in transaction
				cout << "READREPLY: " << msg.value << endl;
				auto it = tmap.find(msg.transID);
				if(it != tmap.end()){
					numReplys = it->second.addReply(msg.value);
					// If all replys have been recieved, then close the transaction and then delete it.
					if(numReplys == NUM_REPLICAS){
						val = it->second.close(&status);
						logAction(it->second.getType(), it->second.getID(), true, it->second.getKey(), val, status); 
						tmap.erase(it);
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
	int currtime = par->getcurrtime();
	for(auto it = tmap.begin(); it != tmap.end(); ){
		// Close the transaction, delete the entry in the map
		if(it->second.getStartTime() + T_CLOSE < currtime){
			cout << "Timeout on transaction  " << it->second.getID() << endl;
			val = it->second.close(&status);
			logAction(it->second.getType(), it->second.getID(), true, it->second.getKey(), val, status);
			// Iterator invalidated, get new iterator
			it = tmap.erase(it);
		}
		else{
			it++;
		}
	}
	if(currtime == 600){
		printHashTable();
	}
}

void MP2Node::printHashTable(){
			
	cout << "PRINT HASH TABLE: " << memberNode->addr.getAddress() << endl;
	for(auto it = ht->hashTable.begin(); it != ht->hashTable.end(); it++){
		cout << "Key: " << it->first << " Value: " << it->second << endl;
	
	}
	cout << endl;
	
}

void MP2Node::sendREPLY(int transID, Address* toAddr, bool status){
	if(transID == STABILIZATION_TID){
		return;
	}
	Message rep(transID, memberNode->addr, REPLY, status); 
	sendMessage(toAddr, &rep);
}

void MP2Node::logAction(MessageType type, int tid, bool isCoord, string key, string value, bool status){
	if(tid == STABILIZATION_TID){
		return;
	}
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
	//TODO: Create more efficient implentation that, in the case of one failure, only sends messages from one remaining replica
	//To do that, need a get distance in ring function. Only send create message to closest node if both are new, for example
	// Get the new hasMyReplicas array
	cout << endl << "Stabilization Protocol: " << memberNode->addr.getAddress() << endl;
	vector<Node> newHMR = findNodes(memberNode->addr.getAddress());
	vector<Node> newHRO = getHRO(&memberNode->addr);

	// Compare newHMR to hasMyReplicas, if they are the same then cvec is empty
	// Otherwise, cvec has Addresses of nodes that are in newHMR but not hasMyReplicas
	// These nodes need to replicate the keys for this node
	vector<Address>* cvec = findNewNeighbors(&hasMyReplicas, &newHMR);
	vector<Address>* dvec = findNewNeighbors(&haveReplicasOf, &newHRO);

	cout << "Sending All Keys to Nodes: " << endl;
	for(int k = 0; k< cvec->size(); k++){
		cout << (*cvec)[k].getAddress() << endl;
	}
	cout << endl;
	bool sendCreate = !cvec->empty();
	bool tryDelete  = !dvec->empty();
	
	size_t lb, ub;
	if(tryDelete){
		getHashBounds(&lb, &ub);
		cout << "Attemping to delete: lb =  " << lb << " ub = " << ub << endl;
	}
	else{
		cout << "No delete neessary... " << endl;
	}
	
	map<string, string>::iterator hIt;
	Message msg(STABILIZATION_TID, memberNode->addr, CREATE, "", ""); //Right constructor not provided by instructors... 
	// Only iterate over the hash table if there are nodes to send messages to
	if(sendCreate || tryDelete){	
		for(hIt = ht->hashTable.begin(); hIt != ht->hashTable.end(); ){
			if(tryDelete){
				size_t curr = hashFunction(hIt->first);
				// Check for deletion
				if(curr <= lb || curr > ub){
					//Iterator invalidated
					cout << memberNode->addr.getAddress() << "deleting key " << hIt->first << endl;
					hIt = ht->hashTable.erase(hIt);
					continue;
				}
			}
			// Update the messages key and values
			if(sendCreate){
				msg.key = hIt->first;
				msg.value = hIt->second;
				// Send the message to everyone who should have the key
				for(int i = 0; i < cvec->size(); i++){
					sendMessage( &((*cvec)[i] ), &msg);	
				}
			}
			// Go to next element
			hIt++;
		}
	}

	hasMyReplicas  = newHMR;
	haveReplicasOf = newHRO; 
	// Clean up memory
	delete cvec;
	delete dvec;
}

void MP2Node::getHashBounds(size_t* lb, size_t* ub){
	//TODO: Make this binary search instead of linear 
	int j;
	bool found = false;
	for(j = 0; j < ring.size(); j++){
		if(ring[j].nodeAddress == memberNode->addr){
			found = true;
			break;
		}
	}
	if(!found) {cout << "NODE NOT FOUND IN RING " << endl;}
	// Compute lower bound on hashes
	j-=NUM_REPLICAS;
	// Deal with wrap around
	if(j < 0 ) {
		j = ring.size() + j;
	}
	*lb = std::min(hashFunction(memberNode->addr.getAddress()), ring[j].nodeHashCode);
	*ub = std::max(hashFunction(memberNode->addr.getAddress()), ring[j].nodeHashCode);
}

/* HRO = haveReplicasOf */
vector<Node> MP2Node::getHRO(Address* addr){
	size_t pos = hashFunction(addr->getAddress());
	vector<Node> newHRO;
	if(ring.size() >=3){
		for(int i = 1; i<ring.size(); i++){
			if(pos == ring.at(i).getHashCode()){
				for(int count = 0; count < NUM_REPLICAS; count++){
					newHRO.emplace_back(ring.at(i));	
					i--;
					if(i < 0){
						i = ring.size()-1;
					}
				}
				
			}
		}
	}
	return newHRO;

}

// Returns a vector of addresses of nodes in newList but not in oldList
vector<Address>* MP2Node::findNewNeighbors(vector<Node>* oldList, vector<Node>* newList){
	vector<Address>* cvec = new vector<Address>;
	// Would like to use an unordered_set or unordred_map, but need to define operator() for Node class to return hash, and also operator ==, but cannot change Node.h for grading...
	// Using an unordered_set will make this O(n), but unfortunatley this must be O(n^2)

	bool found;
	for(int i =	 0; i < newList->size(); i++){
		found = false;
		for(int j = 0; j <oldList->size(); j++){
			if((*newList)[i].nodeAddress == (*oldList)[j].nodeAddress){
				found = true;
				break;
			}
		}
		if(!found){
			cvec->push_back((*newList)[i].nodeAddress);
		}
	}
	return cvec;
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

int Transaction::addReply(string reply){
		cout << "adding reply " << reply << endl;
		numReplys++;
		replys.push_back(reply);	
		if(!reply.empty()){
			numOK++;
		}
		return numReplys;
		
}

int Transaction::addReply(bool transOK){
		cout << "adding reply " << transOK << endl;
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

string MP2Node::type2string(MessageType type){
	switch(type){
		case CREATE:
			return "CREATE";
		case READ:
			return "READ";
		case UPDATE:
			return "UPDATE";
		case DELETE:
			return "DELETE";
		case REPLY:
			return "REPLY";
		case READREPLY:
			return "READREPLY";
		default:
			return "BAD TYPE";
	}
}
