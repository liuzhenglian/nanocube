#include <chrono>

#include <thread>
#include <iostream>
#include <algorithm>
#include <iomanip>
#include <unistd.h>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <string>

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/asio.hpp>

#include "mongoose.h"

#include "Master.hh"
#include "vector.hh"


//-------------------------------------------------------------------------
// Master Request Impl.
//-------------------------------------------------------------------------

MasterRequest::MasterRequest(mg_connection *conn, const std::vector<std::string> &params):
    conn(conn), params(params), response_size(0)
{}


static std::string _content_type[] {
    std::string("application/json")         /* 0 */,
    std::string("application/octet-stream") /* 1 */
};

void MasterRequest::respondJson(std::string msg_content)
{
    //std::cout << "===start MasterRequest::respondJson===" << std::endl;

    const std::string sep = "\r\n";

    std::stringstream ss;
    ss << "HTTP/1.1 200 OK"                << sep
       << "Content-Type: application/json" << sep
       << "Access-Control-Allow-Origin: *" << sep
       << "Content-Length: %d"             << sep << sep
       << "%s";

    response_size = 106 + (int) msg_content.size();
    //response_size = 106 + (int) size; // banchmark data transfer
    // check how a binary stream would work here
    mg_printf(conn, ss.str().c_str(), (int) msg_content.size(), msg_content.c_str());

    
    //std::cout << ss.str() << std::endl;
    //printf(ss.str().c_str(), (int) msg_content.size(), msg_content.c_str());
    //std::cout << "===end MasterRequest::respondJson===" << std::endl;
}

void MasterRequest::respondOctetStream(const void *ptr, int size)
{

    //std::cout << "===start MasterRequest::respondOctetStream===" << std::endl;

    const std::string sep = "\r\n";

    std::stringstream ss;
    ss << "HTTP/1.1 200 OK"                        << sep
       << "Content-Type: application/octet-stream" << sep
       << "Access-Control-Allow-Origin: *"         << sep
       << "Content-Length: %d"                     << sep << sep;

    response_size = 114;
    // check how a binary stream would work here
    mg_printf(conn, ss.str().c_str(), size);


    if (ptr && size > 0) { // unsage access to nullptr
        mg_write(conn, ptr, size);
        response_size += size;
    }

    //std::cout << "===end MasterRequest::respondOctetStream===" << std::endl;
}

void MasterRequest::printInfo()
{
    std::cout << "Request info:" << std::endl;
    const struct mg_request_info *request_info = mg_get_request_info(conn);
    std::cout << request_info->request_method << std::endl;
    std::cout << request_info->uri << std::endl;
    std::cout << request_info->http_version << std::endl;
    std::cout << request_info->query_string << std::endl;
    //std::cout << request_info->post_data << std::endl;
    std::cout << request_info->remote_user << std::endl;
    //std::cout << request_info->log_message << std::endl;
    std::cout << request_info->remote_ip << std::endl;
    std::cout << request_info->remote_port << std::endl;
    //std::cout << request_info->post_data_len << std::endl;
    //std::cout << request_info->status_code << std::endl;
    std::cout << request_info->is_ssl << std::endl;
    std::cout << request_info->num_headers << std::endl;
}

char* MasterRequest::getURI()
{
    const struct mg_request_info *request_info = mg_get_request_info(conn);
    return request_info->uri;
}

const char* MasterRequest::getHeader(char* headername)
{
    return mg_get_header(conn, headername);
}


//-------------------------------------------------------------------------
// MasterException
//-------------------------------------------------------------------------

MasterException::MasterException(const std::string &message):
    std::runtime_error(message)
{}


//-------------------------------------------------------------------------
// Slave
//-------------------------------------------------------------------------

Slave::Slave(std::string address, int port):
    address(address),
    port(port)
{}


//-------------------------------------------------------------------------
// Master
//-------------------------------------------------------------------------

Master::Master(std::vector<Slave> slaves):
  port(29511),
  mongoose_threads(10),
  done(false),
  is_timing(false),
  slaves(slaves)
{}

void Master::processSlave(MasterRequest &request)
{
    //Client: Slave
    //Server: Master
}

void Master::requestSlave(MasterRequest &request, Slave &slave)
{
    //Client: Master
    //Server: Slave

    

    std::cout << "Connecting to slave..." << std::endl;

    boost::asio::io_service io_service;
    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address::from_string(slave.address), slave.port);
    boost::asio::ip::tcp::socket socket(io_service);

    socket.connect(endpoint);

    char buffer[1024];
    std::string uri(request.getURI());
    // if(uri.substr(1,5).compare("query") == 0)
    // {
    //     std::cout << uri << std::endl;
    //     std::cout << "Replacing query for binquery" << std::endl;
    //     uri.replace(1,5, "binquery");
    //     std::cout << uri << std::endl;
    // }
    sprintf(buffer, "GET %s HTTP/1.1\n\n", uri.c_str());
    
    boost::asio::write(socket, boost::asio::buffer(buffer));

    std::string response;
    for (;;)
    {
        char buf[128];
        boost::system::error_code error;

        size_t len = socket.read_some(boost::asio::buffer(buf), error);

        if (error == boost::asio::error::eof)
            break; // Connection closed cleanly by peer.
        else if (error)
            throw boost::system::system_error(error); // Some other error.

        //std::cout.write(buf, len);
        response.insert(response.end(), buf, buf+len);
    }

    socket.close();
    std::cout << "Finished connection..." << std::endl;


    //Parse response
    int pos0 = response.find("Content-Type");
    int pos1 = response.find("\r\n", pos0);
    std::string type = response.substr(pos0+14, pos1-pos0-14);

    pos0 = response.find("Content-Length");
    pos1 = response.find("\r\n", pos0);
    int length = std::stoi(response.substr(pos0+16, pos1-pos0-16));

    std::string content = response.substr(response.length()-length, length);

    // if(uri.substr(1,8).compare("binquery") == 0)
    // {
    //     //std::stringstream is(content);
    //     //std::stringstream os;
    //     //is << content;
    //     //auto result = vector::deserialize(is);
    //     //std::cout << result + result << std::endl;

    //     //std::cout << content << std::endl;
    //     //vector::serialize(content, os);
    //     //std::cout << vector::deserialize(is) << std::endl;
    //     //std::cout << vector::deserialize(os) << std::endl;

    //     //std::cout << os.str() << std::endl;

    //     request.respondOctetStream(content.c_str(), content.size()); 
    // }
    // else{
    //     if(type.compare("application/json") == 0)
    //     {
    //         request.respondJson(content);
    //     }
    //     else
    //     {
    //         request.respondOctetStream(content.c_str(), content.size()); 
    //     } 
    // }
    if(type.compare("application/json") == 0)
    {
        request.respondJson(content);
    }
    else
    {
        request.respondOctetStream(content.c_str(), content.size()); 
    } 


}

void Master::requestAllSlaves(MasterRequest &request)
{

    try
    {
        int i=0;
        for(i = 0; i<slaves.size(); i++)
        {
            requestSlave(request, slaves[i]);
        }
    }
    catch (std::exception& e)
    {
        std::cerr << "Master exception: " << e.what() << std::endl;
    }
    
    
    // //open connection to slave and request data

    // //mpicom.barrier();
    // //std::cout << mpicom.size() << std::endl;
    // boost::mpi::communicator mpicom;
    // boost::mpi::request mpireqs[2];

    // //std::cout << mpicom.rank() << std::endl;

    // std::string outmsg = request.getURI();
    // std::cout << "Master: Requesting " << outmsg << std::endl;
    // mpireqs[0] = mpicom.send(1, 0, outmsg);
    // std::vector<char> inmsg;
    // inmsg.reserve(100000);
    // mpireqs[1] = mpicom.recv(1, 1, inmsg);
    // boost::mpi::wait_all(mpireqs, mpireqs+2);

    // char* data = inmsg.data();
    // ++data;
    // int size = inmsg.size()-1;
    // std::cout << "Master: Reply content " << data << std::endl;

    // if(inmsg.at(0) == '0')
    //     request.respondJson(data, size);
    // else
    //     request.respondOctetStream(data, size);

    //
    //mpicom.barrier();
    // try
    // {
    //     boost::asio::io_service io_service;
    //     tcp::resolver resolver(io_service);

    //     tcp::resolver::query query("localhost", "29512");
    //     tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);

    //     tcp::socket socket(io_service);
    //     boost::asio::connect(socket, endpoint_iterator);

    //     //Send request
    //     acceptor.accept(socket); // 3
    //     std::string message("Hello from server\n");
    //     boost::asio::write(socket, boost::asio::buffer(message)); // 4
    //     socket.close(); // 5

    //     for (;;)
    //     {
    //         char buf[128];
    //         boost::system::error_code error;

    //         size_t len = socket.read_some(boost::asio::buffer(buf), error);

    //         if (error == boost::asio::error::eof)
    //             break; // Connection closed cleanly by peer.
    //         else if (error)
    //             throw boost::system::system_error(error); // Some other error.

    //         std::cout.write(buf, len);
    //     }
    // }
    // catch (std::exception& e)
    // {
    //     std::cerr << e.what() << std::endl;
    // }
        

    // //printf("Connection established.\n");

    // //request.printInfo();

    // //Send request to slave
    // char buffer[1024];
    // sprintf(buffer, "GET %s HTTP/1.1\n\n", request.getURI());
    // send(sockfd, buffer, strlen(buffer), 0);    
    // //std::cout << "Packet: " << buffer << std::endl;
    
    // std::string rec;
    // int bytes_read = 0;
    // int total_read = 0;
    // char type = 0; //0 (json), 1 (octet)
    // int size = 0;
    // do
    // {
    //     bzero(buffer, sizeof(buffer));
    //     bytes_read = recv(sockfd, buffer, sizeof(buffer), 0);
    //     rec.append(buffer, 0, bytes_read);
    //     //printf("%s\n", buffer);
    //     //printf("%d\n", bytes_read);

    //     total_read += bytes_read;

    //     //printf(".. %d\n", buffer);
    //     //printf("%02x %02x %02x %02x\n", buffer[0], buffer[1], buffer[2], buffer[3]);
    //     //printf(".... %d\n", total_read);

    //     if(total_read >= 1)
    //     {
    //         type = atoi(rec.substr(0, 1).c_str());
    //         printf("Type: %s\n", rec.substr(0, 1).c_str());
    //         printf("%s\n", buffer);
    //     }
    //     if(total_read >= 5)
    //     {
    //         printf("%d\n", rec.length());
    //         const char* bytes = rec.substr(1, 5).c_str();
    //         printf("Size: %d\n", bytes);
    //         size = ((unsigned char)bytes[0] << 24) |
    //          ((unsigned char)bytes[1] << 16) |
    //           ((unsigned char)bytes[2] << 8) |
    //            (unsigned char)bytes[3];
    //         printf("Size: %s\n", size);
    //         //printf("%s\n", buffer);
            
    //     }
    // }
    // while(bytes_read > 0);
    // //while ( buffer[bytes_read-1] != '\0' );

    // //rec = rec.substr(0, rec.length()-1);

    // printf("******************Start**************\n");
    // printf("%s\n", rec.c_str());
    // printf("******************End**************\n");

    // /*
    // int pos0 = rec.find("Content-Type");
    // int pos1 = rec.find("\r\n", pos0);
    // std::string type = rec.substr(pos0+14, pos1-pos0-14);
    // //printf("**** %s *****", type.c_str());

    // pos0 = rec.find("Content-Length");
    // pos1 = rec.find("\r\n", pos0);
    // int length = std::stoi(rec.substr(pos0+16, pos1-pos0-16));
    // //printf("!!!! %d !!!!!", length);

    // //pos0 = rec.find("\r\n\r\n");
    // //pos1 = rec.find("\n", pos0);
    // std::string content = rec.substr(rec.length()-length, length);
    // //printf("**** %s *****", content.c_str());

    // //pos0 = rec.find("Content-Type");
    // //pos1 = rec.find("\\n", pos1);
    // //std::string content = rec.substring();
    // */


    // //std::string type = rec.substr(0, 4);
    // //printf("**** %s *****", type.c_str());

    // std::string content = rec.substr(4, rec.length());
    // //printf("**** %s *****", content.c_str());

    // //Close connection
    // close(sockfd);
    // printf("Connection closed.\n");

    // //std::string aux ("{\"menu\": {\"id\": \"file\",\"value\": \"File\",\"popup\": {\"menuitem\": [{\"value\": \"New\", \"onclick\": \"CreateNewDoc()\"},{\"value\": \"Open\", \"onclick\": \"OpenDoc()\"},{\"value\": \"Close\", \"onclick\": \"CloseDoc()\"}]}}}");

    // if(type == 0)
    // {
    //     request.respondJson(content);
    // }
    // else
    // {
    //     printf("******************Start: %d**************\n", content.length());
    //     printf("%s\n", rec.c_str());
    //     printf("******************End: %d**************\n", content.length());
    //     request.respondOctetStream(content.c_str(), content.length());
    // }
    

}

void* Master::mg_callback(mg_event event, mg_connection *conn)
{ // blocks current thread

    if (event == MG_NEW_REQUEST) {

        std::chrono::time_point<std::chrono::high_resolution_clock> t0;
        if (is_timing) {
            t0 = std::chrono::high_resolution_clock::now();
        }

        const struct mg_request_info *request_info = mg_get_request_info(conn);

        std::string uri(request_info->uri);

        std::cout << "Master: Request URI: " << uri << std::endl;

        

        // tokenize on slahses: first should be the address,
        // second the requested function name, and from
        // third on parameters to the functions
        std::vector<std::string> tokens;
        boost::split(tokens, uri, boost::is_any_of("/"));

        MasterRequest request(conn, tokens);

        if (tokens.size() == 0) {
            // std::cout << "Request URI: " << uri << std::endl;
            std::stringstream ss;
            ss << "Master: bad URL: " << uri;
            request.respondJson(ss.str());
            return  (void*) ""; // mark as processed
        }
        else if (tokens.size() == 1) {
            // std::cout << "Request URI: " << uri << std::endl;
            std::stringstream ss;
            ss << "Master: no handler name was provided on " << uri;
            request.respondJson(ss.str());
            return (void*) ""; // mark as processed
        }

        std::string handler_name = tokens[1];
        //std::cout << "Searching handler: " << handler_name << std::endl;


        if (is_timing) {
            //handlers[handler_name](request);
            //requestSlaves(request);
            auto t1 = std::chrono::high_resolution_clock::now();
            uint64_t elapsed_nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t0).count();
            timing_of << currentDateTime() << " " << uri
                      << " " << elapsed_nanoseconds << " ns"
                      << " input " << uri.length()
                      << " output " << request.response_size
                      << std::endl;
            // std::cout << "Request URI: " << uri << std::endl;
        } else {
            std::cout << "Master: Request URI: " << uri << std::endl;
            std::cout << "Master: Request handler: " << handler_name << std::endl;
            //handlers[handler_name](request);
            requestAllSlaves(request);
            //request.printInfo();
            //request.respondJson("blablabla");
        }
        
        return (void*) "";
    }
    return 0;
}

static Master *_master;
void* __mg_master_callback(mg_event event, mg_connection *conn)
{
    return _master->mg_callback(event, conn);
}

void Master::start(int mongoose_threads) // blocks current thread
{

    //boost::mpi::communicator mpicom;
    //std::cout << "**Master: I am process " << mpicom.rank() << " of " << mpicom.size() << "." << std::endl;

    char p[256];
    sprintf(p,"%d",port);
    std::string port_st = p;
    this->mongoose_threads = mongoose_threads;

    // port_st.c_str();
    _master = this; // single master
    // auto callback = std::bind(&Server::mg_callback, this, std::placeholders::_1, std::placeholders::_2);

    std::string mongoose_string = std::to_string(mongoose_threads);
    struct mg_context *ctx;
    const char *options[] = {"listening_ports", port_st.c_str(), "num_threads", mongoose_string.c_str(), NULL};
    ctx = mg_start(&__mg_master_callback, NULL, options);

    if (!ctx) {
        throw MasterException("Couldn't create mongoose context... exiting!");
    }

    // ctx = mg_start(&callback, NULL, options);

    std::cout << "Master server on port " << port << std::endl;
    while (!done) // this thread will be blocked
        std::this_thread::sleep_for(std::chrono::seconds(1));

    // sleep(1);

    mg_stop(ctx);
}

void Master::stop()
{
  done = true;
}

// Get current date/time, format is YYYY-MM-DD.HH:mm:ss
const std::string Master::currentDateTime() {
    time_t     now = time(0);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);
    // Visit http://www.cplusplus.com/reference/clibrary/ctime/strftime/
    // for more information about date/time format
    strftime(buf, sizeof(buf), "%Y-%m-%d_%X", &tstruct);

    return buf;
}
