# Raft-Algorithm
Raft concensus algorithm implemented in c++ and gRPC for voting a new leader in a distributed environment.

**Requirements:**
- Protobuf Grpc must be installed
- Implemented in ubuntu

**Implementation:**
-
A single slave node structure is used containing attributes such as name,filename for its log,address along with term number indication how updated is a node. A config file is used where each node assigns its address when it gets online. Number of maximum nodes is set to 5 but can be changed. Each node uses protobuf for defining the structure of message sent and received from other nodes using gRPC. Majority vote(n/2+1) is used for condition of making a candidate a leader. Each node also stores its own log file. Configuration file is auto created and deleted on successfull voting

**How to run:**
-
- Download the files and make build folder using mkdir -p cmake/build
- cd cmake/build
- cmake ../..
- make -j8
- make logfile folder to store log files
- run 5 slave node using ./ id termnumber status(candidate or follower) portnumber(5000 etc)
- Voting will start as soon as 5 nodes get online
