/*
 * astro_ops.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "astro_ops.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstring>

#include "numpy_utils.hpp"

namespace lithium::numpy {

NumpyResult<py::object> AstroOps::createStarCatalog(
    const std::vector<StarData>& stars) {

    if (!NumpyUtils::isPandasAvailable()) {
        return std::unexpected(NumpyError::ModuleNotFound);
    }

    try {
        py::module pd = py::module::import("pandas");

        // Create lists for each column
        py::list raList, decList, magList, bvList, nameList;
        py::list hipList, pmRaList, pmDecList, plxList;

        for (const auto& star : stars) {
            raList.append(star.ra);
            decList.append(star.dec);
            magList.append(star.magnitude);
            bvList.append(star.bv_color);
            nameList.append(std::string(star.name));
            hipList.append(star.hip_id);
            pmRaList.append(star.proper_motion_ra);
            pmDecList.append(star.proper_motion_dec);
            plxList.append(star.parallax);
        }

        py::dict data;
        data["ra"] = raList;
        data["dec"] = decList;
        data["magnitude"] = magList;
        data["bv_color"] = bvList;
        data["name"] = nameList;
        data["hip_id"] = hipList;
        data["pm_ra"] = pmRaList;
        data["pm_dec"] = pmDecList;
        data["parallax"] = plxList;

        return pd.attr("DataFrame")(data);

    } catch (const py::error_already_set& e) {
        spdlog::error("Failed to create star catalog: {}", e.what());
        return std::unexpected(NumpyError::DataFrameError);
    }
}

NumpyResult<std::vector<StarData>> AstroOps::parseStarCatalog(
    const py::object& df) {

    try {
        std::vector<StarData> stars;

        // Get arrays for each column
        py::array_t<double> raArr = df["ra"].attr("values").cast<py::array_t<double>>();
        py::array_t<double> decArr = df["dec"].attr("values").cast<py::array_t<double>>();
        py::array_t<float> magArr = df["magnitude"].attr("values").cast<py::array_t<float>>();

        auto ra = raArr.unchecked<1>();
        auto dec = decArr.unchecked<1>();
        auto mag = magArr.unchecked<1>();

        size_t count = ra.shape(0);
        stars.reserve(count);

        // Optional columns
        py::array_t<float> bvArr, pmRaArr, pmDecArr, plxArr;
        py::array_t<uint32_t> hipArr;

        bool hasBv = py::hasattr(df, "__getitem__") &&
                     df.attr("columns").attr("__contains__")("bv_color").cast<bool>();
        bool hasHip = df.attr("columns").attr("__contains__")("hip_id").cast<bool>();
        bool hasPmRa = df.attr("columns").attr("__contains__")("pm_ra").cast<bool>();
        bool hasPmDec = df.attr("columns").attr("__contains__")("pm_dec").cast<bool>();
        bool hasPlx = df.attr("columns").attr("__contains__")("parallax").cast<bool>();

        if (hasBv) bvArr = df["bv_color"].attr("values").cast<py::array_t<float>>();
        if (hasHip) hipArr = df["hip_id"].attr("values").cast<py::array_t<uint32_t>>();
        if (hasPmRa) pmRaArr = df["pm_ra"].attr("values").cast<py::array_t<float>>();
        if (hasPmDec) pmDecArr = df["pm_dec"].attr("values").cast<py::array_t<float>>();
        if (hasPlx) plxArr = df["parallax"].attr("values").cast<py::array_t<float>>();

        py::object names;
        bool hasName = df.attr("columns").attr("__contains__")("name").cast<bool>();
        if (hasName) {
            names = df["name"];
        }

        for (size_t i = 0; i < count; ++i) {
            StarData star{};
            star.ra = ra(i);
            star.dec = dec(i);
            star.magnitude = mag(i);

            if (hasBv) star.bv_color = bvArr.at(i);
            if (hasHip) star.hip_id = hipArr.at(i);
            if (hasPmRa) star.proper_motion_ra = pmRaArr.at(i);
            if (hasPmDec) star.proper_motion_dec = pmDecArr.at(i);
            if (hasPlx) star.parallax = plxArr.at(i);

            if (hasName) {
                std::string name = py::str(names.attr("iloc")[py::cast(i)]).cast<std::string>();
                std::strncpy(star.name, name.c_str(), sizeof(star.name) - 1);
                star.name[sizeof(star.name) - 1] = '\0';
            }

            stars.push_back(star);
        }

        return stars;

    } catch (const py::error_already_set& e) {
        spdlog::error("Failed to parse star catalog: {}", e.what());
        return std::unexpected(NumpyError::DataFrameError);
    }
}

NumpyResult<py::array> AstroOps::loadFitsImage(
    const std::filesystem::path& fitsPath, int hdu) {

    try {
        py::module fits = py::module::import("astropy.io.fits");

        py::object hduList = fits.attr("open")(fitsPath.string());
        py::object data = hduList[py::cast(hdu)].attr("data");
        hduList.attr("close")();

        if (data.is_none()) {
            spdlog::error("No data in HDU {} of {}", hdu, fitsPath.string());
            return std::unexpected(NumpyError::FitsError);
        }

        return data.cast<py::array>();

    } catch (const py::error_already_set& e) {
        spdlog::error("Failed to load FITS image: {}", e.what());
        return std::unexpected(NumpyError::FitsError);
    }
}

NumpyResult<std::pair<double, double>> AstroOps::pixelToWorld(
    const py::object& wcs, double x, double y) {

    try {
        py::object result = wcs.attr("all_pix2world")(x, y, 0);
        py::tuple coords = result.cast<py::tuple>();
        return std::make_pair(
            coords[0].cast<double>(),
            coords[1].cast<double>());

    } catch (const py::error_already_set& e) {
        spdlog::error("Failed WCS pixel to world: {}", e.what());
        return std::unexpected(NumpyError::TypeConversionFailed);
    }
}

NumpyResult<std::pair<double, double>> AstroOps::worldToPixel(
    const py::object& wcs, double ra, double dec) {

    try {
        py::object result = wcs.attr("all_world2pix")(ra, dec, 0);
        py::tuple coords = result.cast<py::tuple>();
        return std::make_pair(
            coords[0].cast<double>(),
            coords[1].cast<double>());

    } catch (const py::error_already_set& e) {
        spdlog::error("Failed WCS world to pixel: {}", e.what());
        return std::unexpected(NumpyError::TypeConversionFailed);
    }
}

// Template implementations are in the header for visibility to instantiation code

}  // namespace lithium::numpy
