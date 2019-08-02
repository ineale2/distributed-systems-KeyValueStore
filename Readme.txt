## Distributed Key Value Store
#Project Architecture
This is my project from my Distributed Systems class. It implements a distributed key value store. The distributed systems is emulated by the class EmulNet (EmulNet.h/.cpp), which provides an interface for sending a message to a node with a given address. One layer above tat is the P2P layer, which consists of various nodes. Each node runs its membership protocol/failure detector (MP1Node.h/.cpp) and the key-value store (MP2Node.h/.cpp). One layer above that is the Application class, which creates nodes or causes them to fail and make various key-value store calls. 
#A Note on the Quality of Code
It is important to realize that one of the requirements for this project is that code outside of MP2Node.h/.cpp and MP1Node.h/.cpp could not be modified. This severly limited the design of the system, forced bad style at points, and at many times the provided code is low quality. Let me give some concrete examples:
1) Both MP1Nodes and MP2Nodes have a pointer to a Member class. The Member class has a membership list implemented as a vector of Addresses. A more efficient implementation is to use an unordered_map, to take advantage of constant time lookup throughout the MP1Node class. I used an unordered_map, but in parallel used the vector and updated it only when necessary.
2) The Address class (in Member.h) is a mess. It is intended to store an int and a short (the node id and port), but uses a char array of length 6. Not only does this make the code compiler dependent, it forces ugly conversions from strings representing the address, to Addresses, to <int,short> pairs. 
3) The MP2Node class includes a pointer to a HashTable class, defined in HashTable.h/.cpp. The HashTable class is a wrapper for a map<string,string>, which is actually implemented as a binary search tree. A much more efficient choice would have been an unordered_map<string, Entry>.
4) I designed two new classes: Transaction and Neighbor. These really belong in their own .h/.cpp files, but are written in MP2Node.h/.cpp. 

#How do I run the code?
There are various test cases provided in the testcases directory. Run with ./Application testcases/updatetest for example. Check the dbg.log file that is created to see what goes on in the system. 

## Part 1: Membership Protocol & Failure Detector 
# How does the membership protocol work? 
I chose to implement a flavor of SWIM style failure detection, which is related to gossip-style failure detection. It is weakly consistent, but that is good enough for the requirements in the project (see MP1Specification.pdf). It works by randomly sending a PING to a subset of processes in its membership list. When a process recieves a PING, it responds with an ACK. If a process does not respond in a specified timeout period, the process marks the pinged process as SUSPECTED. If the process remains in the SUSPECTED state for another timeout period, it is removed from the membership protocol. The process is marked as NOT_SUSPECTED if it responds to the ping or if it responds to another process's PING. 

How does a process know if a suspected process responds to another process's message? By gossip-style dissemination of various events in the system. These gossip messages are piggybacked on the PING/ACK messages. Each process has a buffer of events that occur in the system and gossips about them. These events can be process joins, marking a process as suspected, or when a process is no longer suspected. Eventually, these events decay and are removed from the buffer. The reason for this decay is simple: Suppose a process joins and then shortly after fails and is removed from a processes membership list. If the events did not decay, all processes would continuously gossip about the join and suspicion of this process. There would be confusion in the system about its state, which would never be resolved. 

# How did you implement this procotol? What data structures did you use? 

## Part 2: Fault-tolerant Key Value Store
# How does the key value store work?

# How was it implemented? What data structures are used? 


