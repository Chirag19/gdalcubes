/*
   Copyright 2018 Marius Appel <marius.appel@uni-muenster.de>

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include "stream.h"
#include <thread>
#include <iostream>


void stream_chunk(std::shared_ptr<image_collection_cube> in_cube, std::string cmd, uint16_t tid, uint16_t tcount) {

    // TODO: what is better? Creating a new process for each chunk or reusing an interactive process?
    for (uint32_t ic=tid; ic<in_cube->count_chunks(); ic+=tcount) {
        std::shared_ptr<chunk_data> data = in_cube->read_chunk(ic);

        boost::process::opstream in;

        boost::asio::io_service ios;
        std::future<std::vector<char>> outdata;



        //boost::process::child c("Rscript --vanilla -e \"require(gdalcubes); summary(read_stream_as_vector()); write_stream_from_vector();\"", boost::process::std_out > outdata, ios, boost::process::std_in < in, boost::process::env["GDALCUBES_STREAMING"]="1");

        boost::process::child c(cmd, boost::process::std_out > outdata, ios, boost::process::std_in < in, boost::process::env["GDALCUBES_STREAMING"]="1");


        int size[] = {data->size()[0], data->size()[1], data->size()[2], data->size()[3]};
        in.write((char*)(size), sizeof(int)*4);
        in.write(((char*)(data->buf())), sizeof(double)*data->size()[0]*data->size()[1]*data->size()[2]*data->size()[3]);
        in.flush();


        ios.run();

        auto out = outdata.get();
        std::cout << "size :" << out.size() << " bytes received" << std::endl;
        std::cout << ((int*)out.data())[0] << "x" << ((int*)out.data())[1] << "x" << ((int*)out.data())[2] << "x" << ((int*)out.data())[3] << std::endl;
    }
}



void stream_cube::test() {

    std::vector<boost::process::child> children;

    uint16_t nparallel=4;
    std::vector<std::thread> thrds;
    for (uint32_t it=0; it<nparallel; ++it) {
        thrds.push_back(std::thread(stream_chunk,  _in_cube, _cmd, it, nparallel));
    }

    for (uint32_t it=0; it<nparallel; ++it) {
       thrds[it].join();
    }

}


std::shared_ptr<chunk_data> stream_cube::read_chunk(chunkid_t id)  {

    std::shared_ptr<chunk_data> out = std::make_shared<chunk_data>();
    if (id < 0 || id >= count_chunks())
        return out;  // chunk is outside of the cube, we don't need to read anything.



    std::shared_ptr<chunk_data> data = _in_cube->read_chunk(id);

    boost::process::opstream in;

    boost::asio::io_service ios;
    std::future<std::vector<char>> outdata;

    boost::process::child c(_cmd, boost::process::std_out > outdata, ios, boost::process::std_in < in, boost::process::env["GDALCUBES_STREAMING"]="1");


    int size[] = {data->size()[0], data->size()[1], data->size()[2], data->size()[3]};
    in.write((char*)(size), sizeof(int)*4);
    in.write(((char*)(data->buf())), sizeof(double)*data->size()[0]*data->size()[1]*data->size()[2]*data->size()[3]);
    in.flush();


    ios.run();

    std::vector<char> odat = outdata.get();
    std::cout << "size :" << odat.size() << " bytes received" << std::endl;
    std::cout << ((int*)odat.data())[0] << "x" << ((int*)odat.data())[1] << "x" << ((int*)odat.data())[2] << "x" << ((int*)odat.data())[3] << std::endl;


    chunk_size_btyx out_size = {(uint32_t)(((int*)odat.data())[0]), (uint32_t)(((int*)odat.data())[1]), (uint32_t)(((int*)odat.data())[2]),(uint32_t)(((int*)odat.data())[3])};
    out->size(out_size);

    // Fill buffers accordingly
    out->buf(calloc(out_size[0] * out_size[1] * out_size[2] * out_size[3], sizeof(double)));
    memcpy(out->buf(), odat.data() + (4*sizeof(int)), sizeof(double) * out_size[0] * out_size[1] * out_size[2] * out_size[3]);

    return out;
}