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

#include "reduce.h"

std::shared_ptr<chunk_data> reduce_cube::read_chunk(chunkid_t id) {
    std::shared_ptr<chunk_data> out = std::make_shared<chunk_data>();
    if (id < 0 || id >= count_chunks())
        return out;  // chunk is outside of the view, we don't need to read anything.

    coords_nd<uint32_t, 3> size_tyx = chunk_size(id);
    coords_nd<uint32_t, 4> size_btyx = {_bands.count(), 1, size_tyx[1], size_tyx[2]};
    out->size(size_btyx);

    // Fill buffers accordingly
    out->buf(calloc(size_btyx[0] * size_btyx[1] * size_btyx[2] * size_btyx[3], sizeof(double)));

    _r->init(out);

    // iterate over all chunks that must be read from the input cube to compute this chunk
    for (chunkid_t i = id; i < _in_cube->count_chunks(); i += _in_cube->count_chunks_x() * _in_cube->count_chunks_y()) {
        // read chunk i from input cube

        std::shared_ptr<chunk_data> x = _in_cube->read_chunk(i);
        _r->combine(out, x);
    }

    _r->finalize(out);

    return out;
}

void reduce_cube::write_gdal_image(std::string path, std::string format, std::string co) {
    GDALDriver* drv = (GDALDriver*)GDALGetDriverByName(format.c_str());
    if (!drv) {
        throw std::string("ERROR in reduce_cube::write_gdal_image(): Cannot find GDAL driver for given format.");
    }
    // TODO: Check whether driver supports Create()

    // TODO: add create options
    GDALDataset* gdal_out = drv->Create(path.c_str(), _size[3], _size[2], bands().count(), GDT_Float64, NULL);
    //GDALDataset *gdal_out = gtiff_driver->Create((std::to_string(i) + ".tif").c_str(), size_btyx[3], size_btyx[2], band_rels.size(), GDT_Float64, NULL);  // mask band?
    if (!gdal_out) {
        // TODO: Error handling
        throw std::string("ERROR in reduce_cube::write_gdal_image(): cannot create output image");
    }  //        for (uint16_t b=0; b<band_rels.size(); ++b) {

    OGRSpatialReference proj_out;
    proj_out.SetFromUserInput(_st_ref->proj().c_str());
    char* out_wkt;
    proj_out.exportToWkt(&out_wkt);

    double affine[6];
    affine[0] = _st_ref->win().left;
    affine[3] = _st_ref->win().top;
    affine[1] = _st_ref->dx();
    affine[5] = -_st_ref->dy();
    affine[2] = 0.0;
    affine[4] = 0.0;

    gdal_out->SetProjection(out_wkt);
    gdal_out->SetGeoTransform(affine);

    // The following loop seems to be needed for some drivers only
    for (uint16_t b = 0; b < _bands.count(); ++b) {  //            gdal_out->GetRasterBand(b+1)->SetNoDataValue(NAN);
        gdal_out->GetRasterBand(b + 1)->Fill(_bands.get(b).no_data_value);
        gdal_out->GetRasterBand(b + 1)->SetNoDataValue(_bands.get(b).no_data_value);  // why is the no data flag not available in the resulting files?
        // TODO: set scale and offset
    }

    for (uint32_t i = 0; i < count_chunks(); ++i) {
        std::shared_ptr<chunk_data> dat = read_chunk(i);
        bounds_nd<uint32_t, 3> cb = chunk_limits(i);
        for (uint16_t b = 0; b < _bands.count(); ++b) {
            uint32_t yoff = (count_chunks_y() - 1) * _chunk_size[1] - cb.low[1];

            gdal_out->GetRasterBand(b + 1)->RasterIO(GF_Write, cb.low[2], yoff, cb.high[2] - cb.low[2] + 1,
                                                     cb.high[1] - cb.low[1] + 1, ((double*)dat->buf()) + b * dat->size()[2] * dat->size()[3], cb.high[2] - cb.low[2] + 1, cb.high[1] - cb.low[1] + 1,
                                                     GDT_Float64, 0, 0, NULL);
        }
    }

    GDALClose(gdal_out);
}