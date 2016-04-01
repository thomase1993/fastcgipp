#include "fastcgi++/log.hpp"
#include "fastcgi++/sockets.hpp"

#include <vector>
#include <map>
#include <random>
#include <algorithm>
#include <iterator>

const size_t chunkSize=4096;
const unsigned int tranCount=1024;
const unsigned int socketCount=1024;

void client()
{
    Fastcgipp::SocketGroup group;

    struct Buffer
    {
        std::vector<char> buffer;
        std::vector<char> data;
        std::vector<char>::const_iterator send;
        std::vector<char>::iterator receive;
        int count;
    };

    std::map<Fastcgipp::Socket, Buffer> buffers;

    unsigned int maxSockets=0;
    unsigned int sends=0;
    std::random_device rd;
    std::uniform_int_distribution<unsigned long long> intDist;
    unsigned int state;
    
    while(maxSockets<socketCount || buffers.size())
    {
        // Any data waiting for us to recieve?
        {
            Socket socket=group.poll(false);
            if(socket.valid())
            {
                auto buffer = buffers.find(socket);
                if(buffer == buffers.end())
                    FAIL_LOG("Got a valid socket client side that isn't "\
                            "accounted for")
                buffer->second.receive += buffer->first.read(
                        &*buffer->second.receive,
                        &*(buffer->second.buffer.end()-buffer->second.receive));
                if(buffer->second.receive == buffer->second.buffer.end())
                {
                    if(buffer->second.data != buffer->second.buffer)
                        FAIL_LOG("Echoed data does not match that which was "\
                                "sent")

                    if(++buffer->second.count == tranCount)
                    {
                        buffer->first.close();
                        buffers.erase(buffer);
                    }
                    else
                        ++sends;
                }
            }
        }

        // Check for dirty sockets
        for(const auto& buffer: buffers)
            if(!buffer.first.valid())
                FAIL_LOG("A socket has become invalid client side!")

        // Do we initiate a connection, send data or wait?
        if(socketCount-maxSockets || sends)
        {
            std::discrete_distribution<> dist({
                    socketCount-maxSockets,
                    sends});
            state=dist(rd);
        }
        else
            state=2;

        switch(state)
        {
            case 0:
            {
                Fastcgipp::Socket socket = group.connect("localhost", "23987");
                Buffer& buffer(buffers[socket]);
                buffer.buffer.resize(chunkSize);
                buffer.data.resize(chunkSize);
                for(
                        auto i = (unsigned long long*)(&*buffer.data.begin());
                        i < (unsigned long long*)(&*buffer.data.end());
                        ++i)
                    *i = intDist(rd);

                buffer.send = buffer.data.cbegin();
                buffer.receive = buffer.buffer.cbegin();
                buffer.count = 0;
                ++maxSockets;
                ++sends;
                break;
            }
            case 1:
            {
                std::uniform_int_distribution<size_t> dist(0, buffers.size()-1);
                auto pair = buffers.begin();
                std::advance(pair, dist(rd));
                if(!pair->first.valid())
                    FAIL_LOG("Trying to send on an invalid socket client side");

                buffer->second.send += buffer->first.write(
                        &*buffer->second.send,
                        &*(buffer->second.data.end()-buffer->second.send));

                if(buffer->second.receive == buffer->second.buffer.end())
                    --sends;
            }
            default:
                group.poll(true);
                break;
        }
    }
}

void server()
{
    Fastcgipp::SocketGroup group;

    struct Buffer
    {
        std::vector<char> data;
        std::vector<char>::iterator position;
        bool sending;
    };

    std::map<Fastcgipp::Socket, Buffer> buffers;

    while(!done)
    {
        // Clean up dirty sockets
        {
            auto buffer=buffers.begin();

            while(buffer != buffers.end())
            {
                if(buffer->first.valid())
                    ++buffer;
                else
                    buffer = buffers.erase(buffer);
            }
        }

        // Any data waiting for us to recieve?
        {
            Socket socket=group.poll(false);
            if(socket.valid())
            {
                auto buffer = buffers.find(socket);
                if(buffer == buffers.end())
                    FAIL_LOG("Got a valid socket client side that isn't "\
                            "accounted for")
                buffer->second.receive += buffer->first.read(
                        &*buffer->second.receive,
                        &*(buffer->second.buffer.end()-buffer->second.receive));
                if(buffer->second.receive == buffer->second.buffer.end())
                {
                    if(buffer->second.data != buffer->second.buffer)
                        FAIL_LOG("Echoed data does not match that which was "\
                                "sent")

                    if(++buffer->second.count == tranCount)
                    {
                        buffer->first.close();
                        buffers.erase(buffer);
                    }
                    else
                        ++sends;
                }
            }
        }

        // Do we initiate a connection, send data or wait?
        if(socketCount-maxSockets || sends)
        {
            std::discrete_distribution<> dist({
                    socketCount-maxSockets,
                    sends});
            state=dist(rd);
        }
        else
            state=2;

        switch(state)
        {
            case 0:
            {
                Fastcgipp::Socket socket = group.connect("localhost", "23987");
                Buffer& buffer(buffers[socket]);
                buffer.buffer.resize(chunkSize);
                buffer.data.resize(chunkSize);
                for(
                        auto i = (unsigned long long*)(&*buffer.data.begin());
                        i < (unsigned long long*)(&*buffer.data.end());
                        ++i)
                    *i = intDist(rd);

                buffer.send = buffer.data.cbegin();
                buffer.receive = buffer.buffer.cbegin();
                buffer.count = 0;
                ++maxSockets;
                ++sends;
                break;
            }
            case 1:
            {
                std::uniform_int_distribution<size_t> dist(0, buffers.size()-1);
                auto pair = buffers.begin();
                std::advance(pair, dist(rd));
                if(!pair->first.valid())
                    FAIL_LOG("Trying to send on an invalid socket client side");

                buffer->second.send += buffer->first.write(
                        &*buffer->second.send,
                        &*(buffer->second.data.end()-buffer->second.send));

                if(buffer->second.receive == buffer->second.buffer.end())
                    --sends;
            }
            default:
                group.poll(true);
                break;
        }
    }
}

int main()
{
    return 0;
}
