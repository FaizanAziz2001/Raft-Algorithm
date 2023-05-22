
#include <grpcpp/grpcpp.h>
#include "raft.grpc.pb.h"
#include <iostream>
#include <chrono>
#include <unistd.h>
#include <sstream>
#include <string>
#include <thread>
#include <fstream>
#include <unordered_map>
using namespace std;

#define interval 5
// User grpc Functions
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

// import grpc libraries
using grpc::Channel;
using grpc::ClientContext;

// Use prod file structures
using Raft::Ping;
using Raft::Reply;
using Raft::Request;

enum Vote
{
    no = 0,
    yes = 1
};

// node class
class Node
{

    string address;

public:
    // static members to provide global access
    static int tnum;
    static string filename;
    static bool isVoted;
    static int id;
    static string status;
    unordered_map<int, Vote> voters;

    // Constructor
    Node(int node_id, int num, string s, string add)
    {

        this->address = add;

        status = s;
        tnum = num;
        isVoted = false;
        id = node_id;

        // create a log file
        filename = "logfiles/node" + to_string(this->id) + ".txt";
    }

    // getters
    int get_id()
    {
        return id;
    }

    string get_status()
    {
        return status;
    }

    string get_address()
    {
        return this->address;
    }

    string get_filename()
    {
        return filename;
    }

    int get_tnum()
    {
        return tnum;
    }

    // setters
    void set_id(int node_id)
    {
        id = node_id;
    }

    void set_status(string s)
    {
        status = s;
    }

    void set_address(string add)
    {
        this->address = add;
    }
};

// static members initialization
int Node::tnum = 0;
bool Node::isVoted = false;
int Node::id = 0;
string Node::status = "";
string Node::filename = "";

// class for Sever side gprpc
class RaftRecService final : public Ping::Service
{

    // override the function for using our own implementation
public:
    // helper function to assign votes
    void assignVote(Reply *vote_reply, int can_tnum)
    {
        Vote v;

        // if already voted of if tern number of candidate is low, vote no
        if (Node::isVoted == true || can_tnum < Node::tnum)
            v = no;
        else
            v = yes;

        // return  (vote termnumber node id)
        string response = to_string(v) + " " + to_string(Node::tnum) + " " + to_string(Node::id);
        vote_reply->set_data(response);
    }

    // voting functon
    Status RequestForVote(
        ServerContext *context,
        const Request *s,
        Reply *reply) override
    {

        ofstream fout(Node::filename, ios::app);

        // assign reply status
        cout.flush();

        // print the message receive
        cout << "Message received: \n"
             << "---->Term number: " << s->tnum() << "\n---->Log file: " << s->filename() << "\n---->Data: " << s->data() << endl
             << endl;
        // write on log file
        fout << "Term number: " << Node::tnum << ",ID:" << s->nodeid() << ", status: " << s->status() << ", Message received: " << s->data() << endl;

        // check condition for vote
        assignVote(reply, s->tnum());

        // increment term number on receiving
        Node::tnum++;
        return Status::OK;
    }

    // function for receiving status
    Status sendStatus(
        ServerContext *context,
        const Request *s,
        Reply *reply) override
    {

        ofstream fout(Node::filename, ios::app);

        // assign reply status
        cout.flush();

        cout << "Message received:\n"
             << "---->" << s->data() << endl;
        // write on log file
        fout << "Term number: " << Node::tnum << ", ID:" << s->nodeid() << ", status: " << s->status() << ", Message received: " << s->data() << endl;

        // increment term number on receiving
        Node::tnum++;
        return Status::OK;
    }
};

// class for Client grpc
class RafeSendService
{
private:
    // declase a stub for using the service of prod file
    std::unique_ptr<Ping::Stub> stub_;

public:
    RafeSendService(std::shared_ptr<Channel> channel)
        : stub_(Ping::NewStub(channel)) {}

    // function for sending request
    string RequestForVote(Node &node, string message)
    {
        Reply reply;
        Request s;
        ClientContext context;

        // assign parameters
        s.set_nodeid(node.get_id());
        s.set_tnum(node.get_tnum());
        s.set_filename(node.get_filename());
        s.set_data(message);
        s.set_status(node.get_status());

        // send the request to the slave and await for the response
        Status status = stub_->RequestForVote(&context, s, &reply);

        // check for the status of the response message
        if (status.ok())
        {
            return reply.data();
        }
        else
        {
            // std::cout << status.error_code() << ": " << status.error_message()<<
            // std::endl;
            return "fail";
        }
    }

    // function for sending status
    string sendStatus(Node &node, string message)
    {
        Reply reply;
        Request s;
        ClientContext context;

        // assign parameters
        s.set_nodeid(node.get_id());
        s.set_tnum(node.get_tnum());
        s.set_status(node.get_status());
        s.set_data(message);

        // send the request to the slave and await for the response
        Status status = stub_->sendStatus(&context, s, &reply);

        // check for the status of the response message
        if (status.ok())
        {
            return reply.data();
        }
        else
        {
            // std::cout << status.error_code() << ": " << status.error_message()<<
            // std::endl;
            return "fail";
        }
    }
};



// delete config file
void deleteConfig()
{
    string filename = "config.txt";
    std::ifstream file(filename);

    // delete config file after sending
    if (file.good())
    {
        file.close();
        std::remove(filename.c_str());
        std::cout << "File deleted successfully.\n";
    }
}

// register node to config.txt
void registerNode(Node &node)
{
    ofstream fout("config.txt", ios::app);
    fout << "Node" << node.get_id() << " " << node.get_address() << endl;
}

// check all nodes are online
bool checkNodes(int total_nodes)
{
    ifstream fin("config.txt");
    string nodeid, port;
    int count = 0;

    // read from config file and maintain count
    while (fin >> nodeid >> port)
        count++;

    return count == total_nodes;
}

// read nodes from config
vector<string> readNodes(Node &node, int total_nodes)
{
    ifstream fin("config.txt");
    string nodeid, port;
    vector<string> addresses;

    // store addresses in a vector
    for (int i = 0; i < total_nodes; i++)
    {
        fin >> nodeid >> port;

        // Do not add your own port
        if (port != node.get_address())
            addresses.push_back(port);
    }

    return addresses;
}



// send status to a single node
string sendstatus(Node node, string address, string message)
{

    ofstream fout(Node::filename, ios::app);
    // establish a connection to a slave using its address
    RafeSendService client(
        grpc::CreateChannel(address, grpc::InsecureChannelCredentials()));

    fout << "Term number: " << Node::tnum << ",ID:" << node.get_id() << ",status: " << node.get_status() << ", Message sent: " << message << endl;

    string response = client.sendStatus(node, message);
    // increment term number
    node.tnum++;
    return response;
}

// send status to all nodes
void sendStatusToNodes(Node &node, vector<string> addresses)
{

    string message = "Node " + to_string(node.get_id()) + " is the new leader";
    cout << "Message sent to all nodes: \n---->" << message << endl;
    for (int i = 0; i < addresses.size(); i++)
    {
        sendstatus(node, addresses[i], message);
    }
}



// check node vote
bool examineResponse(Node &node, string response, int total_nodes)
{
    istringstream fin(response);
    string rec_vote, rec_tnum, rec_id;
    int majority_vote = (total_nodes / 2) + 1;

    // split string
    fin >> rec_vote >> rec_tnum >> rec_id;

    // if candidate term number is lower,change status
    if (stoi(rec_vote) == no)
    {
        if (stoi(rec_tnum) > node.get_tnum())
        {
            node.set_status("Follower");
        }
    }
    else
    {
        node.voters[stoi(rec_id)] = yes;
    }

    // add voters id and vote in hashmap
    if (node.voters.size() >= majority_vote)
    {
        node.set_status("leader");
        cout << "I am the new leader" << endl
             << endl;
        return true;
    }

    return false;
}

// send data to a node using grpc
string sendmessage(Node node, string address, string message)
{

    ofstream fout(Node::filename, ios::app);
    // establish a connection to a slave using its address
    RafeSendService client(
        grpc::CreateChannel(address, grpc::InsecureChannelCredentials()));

    // send the signal and await for the response
    cout << "Message sent: \n"
         << "---->Term number: " << node.get_tnum() << "\n---->Log file: " << node.get_filename() << "\n---->Data: " << message << endl
         << endl;
    fout << "Term number: " << Node::tnum << ",ID:" << node.get_id() << ",status: " << node.get_status() << ", Message sent: " << message << endl;

    string response = client.RequestForVote(node, message);

    // increment term number
    node.tnum++;
    return response;
}

// send message to all nodes
void sendMessageToNodes(Node &node)
{
    int total_nodes = 5;
    bool newleader = false;

    // keep on checking if all nodes are online
    while (!checkNodes(total_nodes))
    {
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    vector<string> addresses = readNodes(node, total_nodes);
    string message = "Node " + to_string(node.get_id()) + " is running for election";

    // send message to each node

    std::this_thread::sleep_for(std::chrono::seconds(rand() % interval));
    if (node.get_status() == "candidate")
    {

        for (int i = 0; i < addresses.size(); i++)
        {

            std::this_thread::sleep_for(std::chrono::seconds(1));
            string response = sendmessage(node, addresses[i], message);

            // response format (vote,tnum,id)
            if (examineResponse(node, response, total_nodes))
            {
                newleader = true;
                break;
            }
        }
    }

    std::this_thread::sleep_for(std::chrono::seconds(5));
    if (newleader)
    {
        sendStatusToNodes(node, addresses);
        deleteConfig();
    }
}



// function for listening requests
void listenForRequest(Node &node)
{
    // create a service object for using the service of prod file
    RaftRecService service;
    // builder object for establishing a connection between master and slave
    ServerBuilder builder;

    builder.AddListeningPort(node.get_address(), grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    // start the service and await for incoming requests
    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Server listening on port: " << node.get_address() << std::endl
              << endl;

    server->Wait();
}

// testing function
void Run(int argc, char **argv)
{

    // input format (id,tnum,status,address)
    if (argc < 4)
    {
        return;
    }

    string address = "0.0.0.0:";
    address += argv[4];

    Node node(stoi(argv[1]), stoi(argv[2]), argv[3], address);
    registerNode(node);

    // Start your gRPC server on a separate thread
    std::thread server_thread(listenForRequest, ref(node));
    // server_thread.detach();

    // Send gRPC messages on a separate thread
    std::thread client_thread(sendMessageToNodes, ref(node));

    // Wait for both threads to finish

    client_thread.join();
    server_thread.join();
}

// main function
int main(int argc, char **argv)
{
    Run(argc, argv);
    return 0;
}
