using System.Net.Sockets;
using System.Net;
using System.Text;

namespace relay
{
    internal class Program
    {
        static void Main(string[] args)
        {
            Server server = new Server(args.Length > 0 ? args[0] : "127.0.0.1");

            while (true)
            {
                server.Update();
            }
        }
    }

    internal class Server
    {
        private TcpListener sv_recv;
        private TcpListener sv_send;
        public Dictionary<string, NetworkStream> clients = new Dictionary<string, NetworkStream>();

        public Server(string ipAddress)
        {
            sv_recv = new TcpListener(IPAddress.Parse(ipAddress), 80);
            sv_send = new TcpListener(IPAddress.Parse(ipAddress), 443);
            sv_recv.Start();
            sv_send.Start();
            Console.WriteLine($"Server started on {ipAddress}");
        }

        public void Update()
        {
            try
            {
                TcpClient cl_recv = sv_recv.AcceptTcpClient();
                TcpClient cl_send = sv_send.AcceptTcpClient();
                Thread cl_thread = new Thread(HandleClient);
                cl_thread.Start(new TcpClient[] { cl_recv, cl_send });
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error: {ex.Message}");
            }
        }

        private void HandleClient(object? obj)
        {
            TcpClient[] client = (TcpClient[])obj!;
            NetworkStream recv_stream = client[0].GetStream();
            NetworkStream send_stream = client[1].GetStream();
            byte[] recv_buffer = new byte[5000];
            int recv_buffer_size = 0;
            string address = "";

            Console.WriteLine("Client connected.");

            try
            {
                while (true)
                {
                    recv_buffer_size += recv_stream.Read(recv_buffer, recv_buffer_size, recv_buffer.Length - recv_buffer_size);

                    if (recv_buffer_size >= 14)
                    {
                        byte[] source_address = recv_buffer[0..4];
                        byte[] destination_address = recv_buffer[5..8];
                        short port = BitConverter.ToInt16(recv_buffer, 8);
                        int message_length = BitConverter.ToInt32(recv_buffer, 10);

                        if (recv_buffer_size - 14 >= message_length)
                        {
                            if (address == "")
                            {
                                address = Encoding.ASCII.GetString(recv_buffer, 14, message_length);
                                lock (clients)
                                {
                                    clients.Add(address, send_stream);
                                }

                                Console.WriteLine($"Address received: {address}");
                            }
                            else
                            {
                                lock (clients)
                                {
                                    foreach (var otherClient in clients.Values.Where(x => x != send_stream))
                                        otherClient.Write(recv_buffer, 0, message_length + 14);
                                }

                                Console.WriteLine($"Message received: {message_length} bytes");
                            }

                            recv_buffer_size -= message_length + 14;
                            recv_buffer[(message_length + 14)..].CopyTo(recv_buffer, 0);
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Client error: {ex.Message}");
            }
            finally
            {
                lock (clients)
                {
                    if (clients.ContainsKey(address))
                        clients.Remove(address);
                }

                recv_stream.Close();
                client[0].Close();
                send_stream.Close();
                client[1].Close();

                Console.WriteLine("Client disconnected.");
            }
        }
    }
}
