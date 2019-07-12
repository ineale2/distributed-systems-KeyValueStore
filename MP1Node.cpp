/**********************************
 FILE NAME: MP1Node.cpp
 * 
 * DESCRIPTION: Membership protocol run by this Node.
 * 				Definition of MP1Node class functions.
 **********************************/

#include "MP1Node.h"
#include <list>

/*
 * Note: You can change/add any functions in MP1Node.{h,cpp}
 */

/**
 * Overloaded Constructor of the MP1Node class
 * You can add new members to the class if you think it
 * is necessary for your logic to work
 */
MP1Node::MP1Node(Member *member, Params *params, EmulNet *emul, Log *log, Address *address) {
	for( int i = 0; i < 6; i++ ) {
		NULLADDR[i] = 0;
	}
	this->memberNode = member;
	this->emulNet = emul;
	this->log = log;
	this->par = params;
	this->memberNode->addr = *address;
	this->dbTimer = 0;
}

/**
 * Destructor of the MP1Node class
 */
MP1Node::~MP1Node() {}

/**
 * FUNCTION NAME: recvLoop
 *
 * DESCRIPTION: This function receives message from the network and pushes into the queue
 * 				This function is called by a node to receive messages currently waiting for it
 */
int MP1Node::recvLoop() {
    if ( memberNode->bFailed ) {
    	return false;
    }
    else {
    	return emulNet->ENrecv(&(memberNode->addr), enqueueWrapper, NULL, 1, &(memberNode->mp1q));
    }
}

/**
 * FUNCTION NAME: enqueueWrapper
 *
 * DESCRIPTION: Enqueue the message from Emulnet into the queue
 */
int MP1Node::enqueueWrapper(void *env, char *buff, int size) {
	Queue q;
	return q.enqueue((queue<q_elt> *)env, (void *)buff, size);
}

/**
 * FUNCTION NAME: nodeStart
 *
 * DESCRIPTION: This function bootstraps the node
 * 				All initializations routines for a member.
 * 				Called by the application layer.
 */
void MP1Node::nodeStart(char *servaddrstr, short servport) {
    Address joinaddr;
    joinaddr = getJoinAddress();

    // Self booting routines
    if( initThisNode(&joinaddr) == -1 ) {
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "init_thisnode failed. Exit.");
#endif
        exit(1);
    }

    if( !introduceSelfToGroup(&joinaddr) ) {
        finishUpThisNode();
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Unable to join self to group. Exiting.");
#endif
        exit(1);
    }

    return;
}

/**
 * FUNCTION NAME: initThisNode
 *
 * DESCRIPTION: Find out who I am and start up
 */
int MP1Node::initThisNode(Address *joinaddr) {

	memberNode->bFailed = false;
	memberNode->inited = true;
	memberNode->inGroup = false;
    // node is up!
	memberNode->nnb = 0;
	memberNode->heartbeat = 0;
	memberNode->pingCounter = TFAIL;
	memberNode->timeOutCounter = -1;
    initMemberListTable(memberNode);

    return 0;
}

/**
 * FUNCTION NAME: introduceSelfToGroup
 *
 * DESCRIPTION: Join the distributed system
 */
int MP1Node::introduceSelfToGroup(Address *joinaddr) {
	msgTypes *msg;
#ifdef DEBUGLOG
    static char s[1024];
#endif

    if ( 0 == memcmp((char *)&(memberNode->addr.addr), (char *)&(joinaddr->addr), sizeof(memberNode->addr.addr))) {
        // I am the group booter (first process to join the group). Boot up the group
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Starting up group...");
#endif
        memberNode->inGroup = true;
		writeDeltaBuff(memberNode->addr.getAddress(), JOINED);
    }
    else {
        size_t msgsize = sizeof(msgTypes) + sizeof(joinaddr->addr) + 1;
        msg = (msgTypes*) malloc(msgsize * sizeof(char));

        // create JOINREQ message: format of data is {struct Address myaddr}
        *msg = JOINREQ;
        memcpy((char *)(msg+1), &memberNode->addr.addr, sizeof(memberNode->addr.addr));

#ifdef DEBUGLOG
        sprintf(s, "Trying to join...");
        log->LOG(&memberNode->addr, s);
#endif

        // send JOINREQ message to introducer member
        emulNet->ENsend(&memberNode->addr, joinaddr, (char *)msg, msgsize);

        free(msg);
    }

    return 1;

}

/**
 * FUNCTION NAME: finishUpThisNode
 *
 * DESCRIPTION: Wind up this node and clean up state
 */
int MP1Node::finishUpThisNode(){

	delete memberNode;

	return 0;
}

/**
 * FUNCTION NAME: nodeLoop
 *
 * DESCRIPTION: Executed periodically at each member
 * 				Check your messages in queue and perform membership protocol duties
 */
void MP1Node::nodeLoop() {
    if (memberNode->bFailed) {
    	return;
    }

    // Check my messages
    checkMessages();

    // Wait until you're in the group...
    if( !memberNode->inGroup ) {
    	return;
    }

    // ...then jump in and share your responsibilites!
    nodeLoopOps();

    return;
}

/**
 * FUNCTION NAME: checkMessages
 *
 * DESCRIPTION: Check messages in the queue and call the respective message handler
 */
void MP1Node::checkMessages() {
    void *ptr;
    int size;

    // Pop waiting messages from memberNode's mp1q
    while ( !memberNode->mp1q.empty() ) {
    	ptr = memberNode->mp1q.front().elt;
    	size = memberNode->mp1q.front().size;
    	memberNode->mp1q.pop();
    	recvCallBack((void *)memberNode, (char *)ptr, size);
    }
    return;
}

Address MP1Node::processJOINREQ(char* mIn){
	Address addr;

	// Get address of sender from message
	memcpy(&addr, mIn+sizeof(msgTypes), sizeof(addr));
 
	// Update delta buffer and membership map with a new process
	writeDeltaBuff(addr, JOINED);

	return addr;
}

char* MP1Node::createJOINREP(size_t* msgSize){

	// Allocate a message large enough for all the known nodes
	long id, port;
	int numNodes = memberMap.size();
	*msgSize = sizeof(msgTypes) + sizeof(long)*2*numNodes;
	char* mOut = (char*)malloc(*msgSize);

	// Set the message type
	mOut[0]  = (char)JOINREP;

	// Fill out the message with the port & ID of all known nodes
	auto it = memberMap.begin();
	long* nodeData  = (long*)(mOut + sizeof(msgTypes));
	for(int c = 0 ; it != memberMap.end() ; it++){
		// Decompose address string into id and port
		decomposeAddr(it->first, &id, &port);

		// Put data into the message
		nodeData[c++] = id;
		nodeData[c++] = port;
	}
	return mOut;

}

void MP1Node::processJOINREP(char* mIn, int size){

	long* data = (long*)(mIn + sizeof(msgTypes));
	int numNodes = (size - sizeof(msgTypes))/(2*sizeof(long));
	int id;
	short port;
	Address addr;
	// Create an empty entry to be added for various messages
	// Create a vector from the message recieved from introducer
	for(int i = 0; i < numNodes*2; ){
		id   = (int)data[i++];
		port = (short)data[i++];
	
		// Create an Address so it can be added to the grading log
		memcpy(&addr.addr[0], &id,   sizeof(int));
		memcpy(&addr.addr[4], &port, sizeof(short));
		writeDeltaBuff(addr, JOINED);
	}
	// Mark yourself as in the group
	memberNode->inGroup = true;

	// Init timer
	dbTimer = 0;	

	// Tell everyone you've joined!
	writeDeltaBuff(memberNode->addr, JOINED);	
	
}

Address MP1Node::processMessage(msg* mIn){
	// Update membership list based on message in data
	writeDeltaBuff(mIn->dbAddr, mIn->gossipType);	

	return mIn->sender;
}

msg* MP1Node::createMessage(msgTypes msgType){

	// Grab an address from recent change buffer 
	dbTypes db_type;
	Address db_addr = readDeltaBuff(&db_type);	

	// Create the message
	msg* mOut = new msg(msgType, memberNode->addr, db_type, db_addr); 

	return mOut;
}

Address MP1Node::readDeltaBuff(dbTypes* type){

	// Only gossip about events whose nodes have the same sequence number as when pushed into deltaBuff
	while(true){
		// If the delta buffer is empty, then return empty and a dummy address
		if(deltaBuff.empty()){
			*type = EMPTY;
			return memberNode->addr;
		}
		// Reset iterator if end was reached
		if(dbit == deltaBuff.end()){
			dbit = deltaBuff.begin();
		}
		//Make sure not to dereference garbage iterator
		auto it = memberMap.find(dbit->addr);
		if(it == memberMap.end() || dbit->dbseq != it->second.dbseq){
			dbit = deltaBuff.erase(dbit);
		}
		else{
			break;
		}
	}
	
	Address addr(dbit->addr); 
	*type = dbit->dbType;

	// Increment iterator
	dbit++;
	return addr;
}

void MP1Node::writeDeltaBuff(Address addr, dbTypes type){
	 
	long currTime = par->getcurrtime();
	string a = addr.getAddress();
	// Find if this node is in the map
	auto it = memberMap.find(a);
	// Update grading log and membership map
	bool newEvent = false;
	if(type == FAILED){
		// If the node is in the map, write to grading log that the node has been removed, then remove it from map
		if(it != memberMap.end() && memberMap[a].nstat != SUSPECTED){
			
			// Put this node in the queue of suspected processes
			pair<string, long> newEntry(a, currTime + TREMOVE);
			suspects.push_back(newEntry);

			// Mark as suspected in memberMap to prevent duplicate addition
			memberMap[a].nstat = SUSPECTED;
			newEvent = true;
		}
		
	}
	else if(type == JOINED){
		// If the node is not in the map, write to grading log that the node has joined, then add it to map
		if(it == memberMap.end()){
			log->logNodeAdd(&memberNode->addr, &addr);
			//Default constructor has states of NOT_SUSPECTED and NOT_PINGED
			memberMap[a] = nodeData();//NOT_SUSPECTED, NOT_PINGED, 0, 0, 0);
			newEvent = true;
			/* FOR MP2 need to use memberList to get data to MP2Node class... frustratingly inefficient */
			addMember(addr);	
			// Use heartbeat as a sequence number for membership
			memberNode->heartbeat++;
				
		}
	} 
	else if(type == REJUV){
			//If the process is in the suspects queue, then search through suspects and remove
			if(it != memberMap.end() && memberMap[a].nstat == SUSPECTED){
				for(auto curr = suspects.begin(); curr != suspects.end(); ){
					if(curr->first.compare(a) == 0){
						// Erase from the suspects map, and mark it as having responded to (some other node's) ACK
						curr = suspects.erase(curr);
						memberMap[a].nstat  = NOT_SUSPECTED;
						memberMap[a].pstat  = NOT_PINGED;
						memberMap[a].pseq++;
						// If the node still has this process in the suspects queue, then start gossiping about it
						newEvent = true;
					}
					else{
						// This handles the iterator being invalidated when erase is called
						curr++;
					}
				}
			}
	}
	else if(type == EMPTY){
		// Sender process' delta buffer was empty, do nothing
		return;
	}
	else{
		cout << "writeDeltaBuff:: Invalid Argument: type = " << (int)type << " addr =  " << addr.getAddress() << endl;
		throw std::invalid_argument("type not enumerated");		
	}

	// Update delta buffer if this is a new event to the process
	if(newEvent){
		// If the delta buffer is at capacity, remove an element before pushing
		if(deltaBuff.size() >= DELTA_BUFF_SIZE){
			deltaBuff.pop_back();	
		}	
		// Increment deltaBuffer sequence number
		memberMap[a].dbseq++;

		// Push new element into delta buffer
		dbData dbe = dbData(a, type, memberMap[a].dbseq); 
		deltaBuff.push_front(dbe);

		// Reset the iterator
		dbit = deltaBuff.begin();
	}

}

/**
 * FUNCTION NAME: recvCallBack
 *
 * DESCRIPTION: Message handler for different message types
 */
bool MP1Node::recvCallBack(void *env, char *data, int size ) {

	int time = par->getcurrtime();
		
	Address addr;
	size_t msgSize;

	// Get the message type, and switch on the message type
	msg* mIn = (msg*)data;
	msg* mOut;
	switch(mIn->msgType){
		case JOINREQ:
		{
		
			// Add requesting node to membership list, then send full list
			addr = processJOINREQ(data);

			// Create JOINREP message, msgSize is modified with pass by reference
			char* jrMsg = createJOINREP(&msgSize);

			// Reply with the JOINREP message
			emulNet->ENsend(&memberNode->addr, &addr, jrMsg, msgSize);
			
			//clean up memory
			free(jrMsg);

			break;
		}	
		case JOINREP:
		{
			// Create membership list based on the vector in the JOINREP message
			processJOINREP(data, size);

			break;
		}
		case PING:
		{
			// Update this nodes membership list based on the ping message
			addr = processMessage(mIn);

			// Create an ACK message and send it
			mOut = createMessage(ACK);
			emulNet->ENsend(&memberNode->addr, &addr, (char*)mOut, ACK_MSG_SIZE);
			delete mOut;
			
			break;
		}
		case ACK:
		{
			// Update this nodes membership list based on ACK message
			addr = processMessage(mIn);

			// If this process is suspected to be failed, remove it from the suspects queue and send REJUV
			if(memberMap[addr.getAddress()].nstat == SUSPECTED){
				writeDeltaBuff(addr, REJUV);
			}

			// Mark the process as having responded and increment sequence number
			memberMap[addr.getAddress()].pstat = NOT_PINGED;
			memberMap[addr.getAddress()].pseq++;

			break;
		}
		default:
			cout << memberNode->addr.getAddress() << ": INVALID MESSAGE" << endl;
			break;
	}
	//Free the message
	free(data);
	
	return true;	
}

/**
 * FUNCTION NAME: nodeLoopOps
 *
 * DESCRIPTION: Check if any node hasn't responded within a timeout period and then delete
 * 				the nodes
 * 				Propagate your membership list
 */
void MP1Node::nodeLoopOps() {

	Address addr;
	int currTime = par->getcurrtime();
	// Pop stale elements off the delta buffer
	dbTimer++;
	if(dbTimer >= DB_TIMEOUT){
		dbTimer = 0;
		if(!deltaBuff.empty()){
			deltaBuff.pop_back();	
			dbit = deltaBuff.begin();
		}
	} 
	// Check if there are any processes that have exceeded the cleanup time
	if(!suspects.empty() && suspects.front().second <= currTime){
		addr = Address(suspects.front().first);

		// Remove this proess from the member map and write to grading log
		memberMap.erase(suspects.front().first);
		log->logNodeRemove(&memberNode->addr, &addr);
		// Remove this element from the suspects queue
		suspects.pop_front();	
		// For MP2 need to use the memberlist... frustratingly inefficient	
		removeMember(addr);
		// Use heartbeat as a sequence number 	o	
		memberNode->heartbeat++;
	}

	pingData pdata;
	while(!pinged.empty() && pinged.front().expTime < currTime ){
		pdata = pinged.front();
		pinged.pop();
		auto mIt = memberMap.find(pdata.addr);
		/* Check if the sequence number has not been incremented, indicating that no ACK was recieved */
		if(mIt != memberMap.end() && pdata.pseq == mIt->second.pseq){
			writeDeltaBuff(pdata.addr, FAILED);
		}
	} 			

	// Construct PING message, containing an event from the delta buffer
	msg* mOut = createMessage(PING);
	// Chose M random processes to send a PING

	// Select a random element
	int p = rand() % memberMap.size();
	auto it = memberMap.begin();
	std::advance(it, p);
	unsigned long attempts = 0;

	for(int i = 0; i < M && attempts < memberMap.size(); ){
		attempts++;
		addr = it->first;
		// Dont send PING to yourself
		if(addr == memberNode->addr){
			//Don't count this
			continue;
		}
		emulNet->ENsend(&memberNode->addr, &addr, (char*)mOut, PING_MSG_SIZE);

		// Add ping event to the queue if it is not already there
		if(memberMap[addr.getAddress()].pstat == NOT_PINGED){
			pdata.expTime  = currTime + TFAIL;
			pdata.pseq     = it->second.pseq;
			pdata.addr     = it->first;
			pinged.push(pdata);
		}
		// Add processes to PING map
		memberMap[addr.getAddress()].pstat = PINGED;

		// Go to next element, wrapping around if needed
		i++;
		it++;
		if(it == memberMap.end()){
			it = memberMap.begin();
		}
	}
	
	delete mOut;
    return;
}

/**
 * FUNCTION NAME: isNullAddress
 *
 * DESCRIPTION: Function checks if the address is NULL
 */
int MP1Node::isNullAddress(Address *addr) {
	return (memcmp(addr->addr, NULLADDR, 6) == 0 ? 1 : 0);
}

/**
 * FUNCTION NAME: getJoinAddress
 *
 * DESCRIPTION: Returns the Address of the coordinator
 */
Address MP1Node::getJoinAddress() {
    Address joinaddr;

    memset(&joinaddr, 0, sizeof(Address));
    *(int *)(&joinaddr.addr) = 1;
    *(short *)(&joinaddr.addr[4]) = 0;

    return joinaddr;
}

/**
 * FUNCTION NAME: initMemberListTable
 *
 * DESCRIPTION: Initialize the membership list
 */
void MP1Node::initMemberListTable(Member *memberNode) {
	memberNode->memberList.clear();
}

/**
 * FUNCTION NAME: printAddress
 *
 * DESCRIPTION: Print the Address
 */
void MP1Node::printAddress(Address *addr)
{
    printf("%d.%d.%d.%d:%d \n",  addr->addr[0],addr->addr[1],addr->addr[2],
                                                       addr->addr[3], *(short*)&addr->addr[4]) ;    
}

void MP1Node::decomposeAddr(string addr, long* id, long* port){
		size_t pos = addr.find(":");
		*id = (long)stoi(addr.substr(0, pos));
		*port = (long)stoi(addr.substr(pos + 1, addr.size()-pos-1));
}

void MP1Node::addMember(Address addr){
	long id, port;
	decomposeAddr(addr.getAddress(), &id, &port);	
	memberNode->memberList.push_back(MemberListEntry((int)id, short(port)));	
}

void MP1Node::removeMember(Address addr){
	long id, port;
	decomposeAddr(addr.getAddress(), &id, &port);
	auto it = memberNode->memberList.begin();	
	for( ; it != memberNode->memberList.end(); it++){
		if(it->id == id && it->port == port){
			// Found the node to delete
			memberNode->memberList.erase(it);
			return;
		}	
	}
	throw std::invalid_argument("MEMBER_NOT_FOUND");		

}
void MP1Node::printMemberList(){
	cout << memberNode->addr.getAddress() << " printing memberlist: " << endl;
	int i;
	for(i = 0; i< memberNode->memberList.size(); i++){
		cout << "id: " << memberNode->memberList[i].id << " port: " << memberNode->memberList[i].port << endl;
	} 
	cout << "end memberList " << endl << endl;
}
