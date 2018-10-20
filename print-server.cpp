#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <vector>

using boost::asio::ip::tcp;
namespace po = boost::program_options;

class session : public std::enable_shared_from_this<session> {
  private:
    static constexpr std::size_t k_buffer_len = 1024;

    tcp::socket                         d_socket;
    std::vector<std::unique_ptr<char> > d_buffers;  // one buffer split into many similar sections
    std::size_t                         d_next_pos; // in the locical buffer d_buffers
    std::size_t                         d_message_start;

  public:
    session(tcp::socket socket)
    : d_socket(std::move(socket))
    , d_next_pos{0}
    , d_message_start{0}
    {
      d_buffers.emplace_back(new char[k_buffer_len]);
    }

    void start() { do_read_msg(); }

  private:
    char *get_read_buffer()
    {
        std::size_t buffer_index  = d_next_pos / k_buffer_len;
        std::size_t buffer_offset = d_next_pos % k_buffer_len;

        return d_buffers[buffer_index].get() + buffer_offset;
    }

    std::size_t get_remaining_space()
    {
        std::size_t buffer_offset = d_next_pos % k_buffer_len;

        return k_buffer_len - buffer_offset;
    }

    std::uint64_t get_payload_length()
    {
        std::size_t to_read = sizeof(std::uint64_t);
        char        length[to_read];

        std::size_t available =
            k_buffer_len - (d_message_start % k_buffer_len);

        if (available >= 8) {
            char *buffer = d_buffers[d_message_start / k_buffer_len].get();
            char *start  = +buffer + (d_message_start % k_buffer_len);
            char *end    = start + 8;
            std::copy(start, end, length);
            std::uint64_t payload_length =
                be64toh(*reinterpret_cast<std::uint64_t *>(length));
        }
        else {
            // payload lenght is split between buffer segments
            throw std::runtime_error("this is harder");
        }
    }

    void do_read_msg()
    {
        // read length
        auto self(shared_from_this());

        std::function<void(boost::system::error_code ec, std::size_t len)>
            data_read;

        data_read = [this, self](auto ec, auto len) {
            if (!ec) {
                std::cout << "read " << len << " bytes\n";
                d_next_pos += len;
                if (d_next_pos / k_buffer_len > d_buffers.size() - 1) {
                    d_buffers.emplace_back(new char[k_buffer_len]);
                }
                if (d_next_pos < sizeof(std::uint64_t)) {
                   this->do_read_msg();
                   return;
                }
                std::uint64_t payload_length = this->get_payload_length();
                std::cout << "payload_length = " << payload_length << '\n';
                if (d_next_pos - d_message_start < payload_length) {
                    this->do_read_msg();
                    std::cout << "awaiting more of the message\n";
                    return;
                }
                std::cout << "got full message\n";
            }
        };

        d_socket.async_read_some(
               boost::asio::buffer(get_read_buffer(), get_remaining_space()) ,
               data_read);
    }
};

class server {
  private:
    tcp::acceptor d_acceptor;

  public:
    server(boost::asio::io_context& io_context, short port)
    : d_acceptor(io_context, tcp::endpoint(tcp::v4(), port))
    {
        do_accept();
    }

  private:
    void do_accept()
    {
        std::function<void(boost::system::error_code ec, tcp::socket socket)>
            on_accept;

        on_accept = [this](auto ec, auto socket) {
            std::cout << "Accepted connection\n";
            if (!ec) {
                auto new_session =
                    std::make_shared<session>(std::move(socket));
                new_session->start();
            }

            this->do_accept();
        };

        d_acceptor.async_accept(on_accept);
    }
};

class client {
private:
  tcp::socket d_socket;

public:
  client(boost::asio::io_context& io_context,
         std::string              hostname,
         std::string              port)
  : d_socket(io_context)
  {
    tcp::resolver resolver(io_context);
    boost::asio::connect(d_socket, resolver.resolve(hostname, port));
  }
  
  void send()
  {
      std::uint64_t  length = 1234;
      std::uint64_t  n_length = htobe64(length);  // host to big endian 64 bit
      char         *request  = reinterpret_cast<char *>(&n_length);
      std::size_t   request_len = sizeof(n_length);
      
      boost::asio::write(d_socket, boost::asio::buffer(request, request_len));

      char          payload[length];

      std::uint8_t i = 0;
      for (char *cur = payload; cur < payload + length; cur++) {
          *cur = i;
      }

      boost::asio::write(d_socket, boost::asio::buffer(payload, length));
  }
};

int main(int argc, char *argv[])
{
    po::options_description desc("Proto client/server");
    desc.add_options()("help", "print this help information");
    desc.add_options()("hostname", po::value<std::string>(), "name to use when connecting");
    desc.add_options()("port", po::value<std::string>(), "port to listen / connect with");
    desc.add_options()("mode", po::value<int>(), "mode: server == 0, client == 1");

    po::variables_map args;
    try {
        po::store(po::parse_command_line(argc, argv, desc), args);
    }
    catch (boost::program_options::unknown_option& unknown) {
      std::cout << desc << '\n';
      return 1;
    }

    if (args.count("help")) {
        std::cout << desc << '\n';
        return 1;
    }

    std::string port     = "4242";
    std::string hostname = "localhost";
    int         mode     = 0;

    if (args.count("port")) {
        port = args["port"].as<short>();
    }
    if (args.count("hostname")) {
        hostname = args["hostname"].as<std::string>();
    }
    if (args.count("mode")) {
        mode = args["mode"].as<int>();
    }

    try {
        boost::asio::io_context io_context;

        if (mode == 0) {
            server s(io_context, std::atoi(port.c_str()));
            io_context.run();
        }
        else if (mode == 1) {
            client c(io_context, hostname, port);
            c.send(); 
            io_context.run();
        }
    }
    catch (std::exception& e) {
        return 1;
    }
    return 0;
}
