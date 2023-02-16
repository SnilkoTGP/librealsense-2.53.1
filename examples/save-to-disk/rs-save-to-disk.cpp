// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015-2017 Intel Corporation. All Rights Reserved.

#include <librealsense2/rs.hpp> // Include RealSense Cross Platform API

#include <fstream>              // File IO
#include <iostream>             // Terminal IO
#include <sstream>              // Stringstreams

#include <iostream>
#include <WS2tcpip.h>
#include <string>

#include <stdio.h>
#include <direct.h>
#include <errno.h>

#pragma comment (lib, "ws2_32.lib")

// 3rd party header for writing png files
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// Helper function for writing metadata to disk as a csv file
void metadata_to_csv(const rs2::frame& frm, const std::string& filename);

// This sample captures 30 frames and writes the last frame to disk.
// It can be useful for debugging an embedded system with no display.
int main(int argc, char* argv[]) try
{
    // Declare depth colorizer for pretty visualization of depth data
    rs2::colorizer color_map;

    // Declare RealSense pipeline, encapsulating the actual device and sensors
    rs2::pipeline pipe;

    // Start streaming with default recommended configuration
    pipe.start();

    //Create new process folder
    int idx = 0;
    char sIdx[10];
    int retval;
    std::string cPath, vPath;
    cPath.assign("\\");
    cPath.append("testFolder2");
    cPath.append("\\");
    cPath.append("testImageFolder");
    vPath.assign(cPath.data());
    sprintf(sIdx, "%i", idx++);
    vPath.append(sIdx);
    do
    {
        printf("Make directory postponed.\n");
        vPath.assign(cPath.data());
        sprintf(sIdx, "%i", idx++);
        vPath.append(sIdx);
    } while (retval = _mkdir(vPath.data()) == -1);
    printf("Make directory succeeded.\n");

    // Initialze winsock
    WSADATA wsData;
    WORD ver = MAKEWORD(2, 2);

    int wsOk = WSAStartup(ver, &wsData);
    if (wsOk != 0)
    {
        std::cerr << "Can't Initialize winsock! Quitting" << std::endl;
        return 0;
    }

    // Create a socket
    SOCKET listening = socket(AF_INET, SOCK_STREAM, 0);
    if (listening == INVALID_SOCKET)
    {
        std::cerr << "Can't create a socket! Quitting" << std::endl;
        return 0;
    }

    // Bind the ip address and port to a socket
    sockaddr_in hint;
    hint.sin_family = AF_INET;
    hint.sin_port = htons(40000);
    inet_pton(AF_INET, "192.168.0.124", &hint.sin_addr.S_un.S_addr);
    bind(listening, (sockaddr*)&hint, sizeof(hint));

    // Tell Winsock the socket is for listening 
    listen(listening, SOMAXCONN);

    // Wait for a connection
    sockaddr_in client;
    int clientSize = sizeof(client);

    SOCKET clientSocket = accept(listening, (sockaddr*)&client, &clientSize);

    char host[NI_MAXHOST];		// Client's remote name
    char service[NI_MAXSERV];	// Service (i.e. port) the client is connect on

    ZeroMemory(host, NI_MAXHOST); // same as memset(host, 0, NI_MAXHOST);
    ZeroMemory(service, NI_MAXSERV);

    if (getnameinfo((sockaddr*)&client, sizeof(client), host, NI_MAXHOST, service, NI_MAXSERV, 0) == 0)
    {
        std::cout << host << " connected on port " << service << std::endl;
    }
    else
    {
        inet_ntop(AF_INET, &client.sin_addr, host, NI_MAXHOST);
        std::cout << host << " connected on port " <<
            ntohs(client.sin_port) << std::endl;
    }

    // Close listening socket
    closesocket(listening);

    // While loop: accept the message from client
    char buf[4096], buf2[4096];

    while (true)
    {
        ZeroMemory(buf, 4096);
        ZeroMemory(buf2, 4096);


        // Wait for client to send data
        int bytesReceived = recv(clientSocket, buf, 4096, 0);
        if (bytesReceived == SOCKET_ERROR)
        {
            std::cerr << "Error in recv(). Quitting" << std::endl;
            break;
        }

        if (bytesReceived == 0)
        {
            std::cout << "Client disconnected " << std::endl;
            break;
        }

        std::cout << std::string(buf, 0, bytesReceived) << std::endl;
        if (std::string(buf) == "0_UR10_Connected_to_Server")
        {
            strncpy_s(buf2, "Start", 10);
        }
        else
        {
            strncpy_s(buf2, "Go", 10);

            // Capture 30 frames to give autoexposure, etc. a chance to settle
            for (auto i = 0; i < 1; ++i) pipe.wait_for_frames();

            // Wait for the next set of frames from the camera. Now that autoexposure, etc.
            // has settled, we will write these to disk
            for (auto&& frame : pipe.wait_for_frames())
            {
                // We can only save video frames as pngs, so we skip the rest
                if (auto vf = frame.as<rs2::video_frame>())
                {

                    auto stream = frame.get_profile().stream_type();
                    // Use the colorizer to get an rgb image for the depth stream
                    if (vf.is<rs2::depth_frame>()) vf = color_map.process(frame);

                    // Write images to disk
                    std::stringstream png_file;
                    png_file << "C:" << vPath.data() << "//" << "MAXXgripPic-" << buf << vf.get_profile().stream_name() << ".png";
                    //png_file << "C:" << vPath.data() << "//" << "MAXXgripPic" << bytesReceived << vf.get_profile().stream_name() << ".png";
                    //png_file << "C://testFolder2//testImageFolder1//rs-save-to-disk-output-" << vf.get_profile().stream_name() << ".png";
                    stbi_write_png(png_file.str().c_str(), vf.get_width(), vf.get_height(),
                        vf.get_bytes_per_pixel(), vf.get_data(), vf.get_stride_in_bytes());
                    std::cout << "Saved " << png_file.str() << std::endl;

                    // Record per-frame metadata for UVC streams
                    std::stringstream csv_file;
                    csv_file << "rs-save-to-disk-output-" << vf.get_profile().stream_name()
                        << "-metadata.csv";
                    metadata_to_csv(vf, csv_file.str());
                }
            }
        }
        int sl = strlen(buf2);
        send(clientSocket, buf2, sl + 1, 0);
    }

    // Close the socket
    closesocket(clientSocket);

    // Cleanup winsock
    WSACleanup();

    system("pause");



    return EXIT_SUCCESS;
}
catch (const rs2::error& e)
{
    std::cerr << "RealSense error calling " << e.get_failed_function() << "(" << e.get_failed_args() << "):\n    " << e.what() << std::endl;
    return EXIT_FAILURE;
}
catch (const std::exception& e)
{
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}

void metadata_to_csv(const rs2::frame& frm, const std::string& filename)
{
    std::ofstream csv;

    csv.open(filename);

    //    std::cout << "Writing metadata to " << filename << endl;
    csv << "Stream," << rs2_stream_to_string(frm.get_profile().stream_type()) << "\nMetadata Attribute,Value\n";

    // Record all the available metadata attributes
    for (size_t i = 0; i < RS2_FRAME_METADATA_COUNT; i++)
    {
        if (frm.supports_frame_metadata((rs2_frame_metadata_value)i))
        {
            csv << rs2_frame_metadata_to_string((rs2_frame_metadata_value)i) << ","
                << frm.get_frame_metadata((rs2_frame_metadata_value)i) << "\n";
        }
    }

    csv.close();
}