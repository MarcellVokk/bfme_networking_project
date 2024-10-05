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
            string address = "";

            Console.WriteLine("Client connected.");

            try
            {
                while (true)
                {
                    byte[] lengthBuffer = new byte[4];
                    int bytesRead = recv_stream.Read(lengthBuffer, 0, lengthBuffer.Length);

                    if (bytesRead == 0)
                        break;

                    if (bytesRead < 4)
                        throw new Exception("Failed to read the full message length.");

                    int messageLength = BitConverter.ToInt32(lengthBuffer, 0);
                    Console.WriteLine($"Message length: {messageLength} bytes");

                    byte[] messageBuffer = new byte[messageLength];
                    int totalBytesRead = 0;

                    while (totalBytesRead < messageLength)
                    {
                        int currentBytesRead = recv_stream.Read(messageBuffer, totalBytesRead, messageLength - totalBytesRead);
                        if (currentBytesRead == 0) throw new Exception("Client disconnected before the full message was received.");
                        totalBytesRead += currentBytesRead;
                    }

                    if (address == "")
                    {
                        address = Encoding.ASCII.GetString(messageBuffer);
                        Console.WriteLine($"Address received: {address}");

                        clients.Add(address, send_stream);
                    }
                    else
                    {
                        Console.WriteLine($"Message received: {messageBuffer.Length} bytes");

                        foreach (var otherClient in clients.Values.Where(x => x != send_stream))
                            otherClient.Write(BitConverter.GetBytes(messageLength).Concat(messageBuffer).ToArray());
                    }
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Client error: {ex.Message}");
            }
            finally
            {
                if (clients.ContainsKey(address))
                    clients.Remove(address);

                recv_stream.Close();
                client[0].Close();
                send_stream.Close();
                client[1].Close();

                Console.WriteLine("Client disconnected.");
            }
        }
    }
}
