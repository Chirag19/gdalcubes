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

#ifndef IMAGE_COLLECTION_H
#define IMAGE_COLLECTION_H

#include <gdal_priv.h>
#include <ogr_spatialref.h>
#include <boost/date_time.hpp>
#include "collection_format.h"
#include "datetime.h"

template <typename Ta>
struct bounds_2d {
    Ta left, bottom, top, right;
    static bool intersects(bounds_2d<Ta> a, bounds_2d<Ta> b) {
        return (
            a.right >= b.left &&
            a.left <= b.right &&
            a.top >= b.bottom &&
            a.bottom <= b.top);
    }
    static bool within(bounds_2d<Ta> a, bounds_2d<Ta> b) {
        return (
            a.left >= b.left &&
            a.right <= b.right &&
            a.top <= b.top &&
            a.bottom >= b.bottom);
    }
    static bool outside(bounds_2d<Ta> a, bounds_2d<Ta> b) {
        return !intersects(a, b);
    }
    static bounds_2d<Ta> union2d(bounds_2d<Ta> a, bounds_2d<Ta> b) {
        bounds_2d<Ta> out;
        out.left = fmin(a.left, b.left);
        out.right = fmax(a.right, b.right);
        out.bottom = fmin(a.bottom, b.bottom);
        out.top = fmax(a.top, b.top);
        return out;
    }

    static bounds_2d<Ta> intersection(bounds_2d<Ta> a, bounds_2d<Ta> b) {
        bounds_2d<Ta> out;
        out.left = fmax(a.left, b.left);
        out.right = fmin(a.right, b.right);
        out.bottom = fmin(a.bottom, b.bottom);
        out.top = fmax(a.top, b.top);
        return out;
    }

    bounds_2d<Ta> transform(std::string srs_from, std::string srs_to) {
        OGRSpatialReference srs_in;
        OGRSpatialReference srs_out;
        srs_in.SetFromUserInput(srs_from.c_str());
        srs_out.SetFromUserInput(srs_to.c_str());

        if (srs_in.IsSame(&srs_out)) {
            return *this;
        }

        OGRCoordinateTransformation* coord_transform = OGRCreateCoordinateTransformation(&srs_in, &srs_out);

        Ta x[4] = {left, left, right, right};
        Ta y[4] = {top, bottom, top, bottom};

        if (coord_transform == NULL || !coord_transform->Transform(4, x, y)) {
            throw std::string("ERROR: coordinate transformation failed.");
        }

        Ta xmin = std::numeric_limits<Ta>::is_integer ? std::numeric_limits<Ta>::max() : std::numeric_limits<Ta>::max();
        Ta ymin = std::numeric_limits<Ta>::is_integer ? std::numeric_limits<Ta>::max() : std::numeric_limits<Ta>::max();
        Ta xmax = std::numeric_limits<Ta>::is_integer ? std::numeric_limits<Ta>::min() : -std::numeric_limits<Ta>::max();
        Ta ymax = std::numeric_limits<Ta>::is_integer ? std::numeric_limits<Ta>::min() : -std::numeric_limits<Ta>::max();
        for (uint8_t k = 0; k < 4; ++k) {
            if (x[k] < xmin) xmin = x[k];
            if (y[k] < ymin) ymin = y[k];
            if (x[k] > xmax) xmax = x[k];
            if (y[k] > ymax) ymax = y[k];
        }

        bounds_2d<Ta> in_extent;
        left = xmin;
        right = xmax;
        top = ymax;
        bottom = ymin;

        return *this;
    }
};

template <typename T>
struct coords_2d {
    T x, y;
};

//template <typename T> using coords_nd = std::vector<T>;
template <typename T, uint16_t N>
using coords_nd = std::array<T, N>;

struct coords_st {
    coords_2d<double> s;
    datetime t;
};

struct bounds_st {
    bounds_2d<double> s;
    datetime t0;
    datetime t1;
};

/**
 * @note copy construction and assignment are deleted because the sqlite must not be shared (handle will be closed in destructor). Instrad, use
 * std::shared_ptr<image_collection> to share the whole image collection resource if needed.
  */
class image_collection {
   public:
    /**
     * Constructs an empty image collection with given format
     * @param format
     */
    image_collection(collection_format format);

    /**
     * Opens an existing image collection from a file
     * @param filename
     */
    image_collection(std::string filename);

    ~image_collection() {
        if (_db) {
            sqlite3_close(_db);
            _db = nullptr;
        }
    }

    image_collection(const image_collection&) = delete;
    void operator=(const image_collection&) = delete;

    // move constructor
    image_collection(image_collection&& A) {
        _db = A._db;
        _filename = A._filename;
        _format = A._format;
    }

    static image_collection create(collection_format format, std::vector<std::string> descriptors, bool strict = true);

    std::string to_string();

    void add(std::vector<std::string> descriptors, bool strict = true);
    void add(std::string descriptor);

    void write(const std::string filename);

    /**
     * Stores an image collection as a new temporary database, same as image_collection::write("")
     */
    inline void temp_copy() { return write(""); }

    /**
     * Check if the database schema is complete, i.e., the database is ready for e.g. adding images.
     * @return true, if the database is OK, false otherwise (e.g. if a table is missing or there are no bands)
     * @note NOT YET IMPLEMENTED
     */
    bool is_valid();

    /**
    * Check if the database has no images
    * @return true, if the database is OK, false otherwise
    * @note NOT YET IMPLEMENTED
    */
    bool is_empty();

    /**
     * Check if the collection has gdalrefs for all image X band combinations, i.e. whether there is no image with
     * missing bands.
     * @return true, if the collection is complete, false otherwise.
     * @note NOT YET IMPLEMENTED
     */
    bool is_complete();

    /**
     * Removes images and corresponding gdal dataset references captured before start or after end from the database.
     * @param start Posix time representing the start datetime of the range
     * @param end Posix time representing the end datetime of the range
     * @note This operations works in-place, it will overwrite the opened database file. Consider calling `temp_copy()` to create
     * a temporary copy of the database before.
     * @note NOT YET IMPLEMENTED
     */
    void filter_datetime_range(boost::posix_time::ptime start, boost::posix_time::ptime end);

    /**
     * @see image_collection::filter_datetime_range(boost::posix_time::ptime start, boost::posix_time::ptime end)
     * @param start start datetime as ISO8601 string
     * @param end end datetime as ISO8601 string
     */
    void filter_datetime_range(std::string start, std::string end) {
        filter_datetime_range(boost::posix_time::from_iso_string(start), boost::posix_time::from_iso_string(end));
    }

    /**
     * Removes unneeded bands and the corresponding gdal dataset references.
     * @param bands std::vector of band names, all bands not in the list will be removed from the collection.
     * @note This operations works in-place, it will overwrite the opened database file. Consider calling `temp_copy()` to create
     * a temporary copy of the database before.
     * @note NOT YET IMPLEMENTED
     */
    void filter_bands(std::vector<std::string> bands);

    /**
    * Removes images that are located outside of a given rectangular range.
    * @param range the coordinates of the rectangular spatial range
    * @param proj Projection string is a format readable by GDAL (typically proj4 or proj5).
    * @note This operations works in-place, it will overwrite the opened database file. Consider calling `temp_copy()` to create
    * a temporary copy of the database before.
    * @note NOT YET IMPLEMENTED
    */
    void filter_spatial_range(bounds_2d<double> range, std::string proj);

    uint16_t count_bands();
    uint32_t count_images();
    uint32_t count_gdalrefs();

    struct find_result {
        std::string image_name;
        std::string descriptor;
        std::string datetime;
        std::string band_name;
        uint16_t band_num;
    };
    std::vector<find_result> find_with(bounds_2d<double> range, std::string proj, std::vector<std::string> bands, std::string start, std::string end);

    struct band_info {
        uint32_t id;
        std::string name;
        GDALDataType type;
        double offset;
        double scale;
        std::string unit;
    };

    std::vector<image_collection::band_info> get_bands();

    /**
     * Derive the size of a pixel for one or all bands in bytes
     * @param band band identifier, if emtpy the sum of all bands is used
     * @return the size of a pixel with one or all bands or zero if the specified band does not exist.
     */
    uint16_t pixel_size_bytes(std::string band = "");

    /**
     * Derive the spatial and temporal extent of all images
     * @return the spatial and temporal extent
     */
    bounds_st extent();

   protected:
    collection_format _format;
    std::string _filename;
    sqlite3* _db;
};

#endif  //IMAGE_COLLECTION_H
