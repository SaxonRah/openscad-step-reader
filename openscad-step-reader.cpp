/*
 * Copyright 2019 Assaf Gordon <assafgordon@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 */
#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <stdexcept>
#include <Windows.h>

 // OpenCASCADE headers
#include <STEPControl_Reader.hxx>
#include <StlAPI_Writer.hxx>
#include <BRepMesh_IncrementalMesh.hxx>

// Project headers
#include "triangle.h"
#include "tessellation.h"
#include "openscad-triangle-writer.h"
#include "explore-shape.h"

// Windows-compatible command-line parsing
struct Option {
    const char* name;
    int has_arg;
    int* flag;
    int val;
};

// Define output format enum at file scope
enum OutputFormat {
    OUT_UNDEFINED,
    OUT_STL_ASCII,
    OUT_STL_SCAD,
    OUT_STL_FACES,
    OUT_STL_OCCT,
    OUT_EXPLORE
};

static Option options[] = {
    {"help",      0, 0, 'h'},
    {"version",   0, 0, 'V'},
    {"stl-ascii", 0, 0, 'a'},
    {"stl-scad",  0, 0, 's'},
    {"stl-faces", 0, 0, 'f'},
    {"stl-occt",  0, 0, 'o'},
    {"stl-lin-tol", 1, 0, 'L'},
    {"explore",   0, 0, 'e'},
    {0, 0, 0, 0}
};

void show_help()
{
    std::cout << "openscad-step-reader\n"
        "\n"
        "A proof-of-concept program for STEP/OpenSCAD integration\n"
        "\n"
        "usage: openscad-step-reader [options] INPUT.STEP\n"
        "\n"
        "Output is written to STDOUT.\n"
        "\n"
        "options are:\n"
        "   -h, --help         this help screen\n"
        "   -V, --version      version information\n"
        "\n"
        "   -o, --stl-occt     convert the input STEP file into ASCII STL file\n"
        "                      using OpenCASCADE code. This should be the baseline\n"
        "                      when debugging/troubleshooting incorrect outputs.\n"
        "\n"
        "   -a, --stl-ascii    convert the input STEP file into custom ASCII STL file,\n"
        "                      using our code. This is a good test to check mesh\n"
        "                      triangulation code. EXCEPT for the 'normal' values\n"
        "                      which are not produced, the vertex values should be\n"
        "                      equivalent to those with --stl-occt.\n"
        "\n"
        "   -s, --stl-scad     convert the input STEP file into SCAD code, containing\n"
        "                      a single 'polyhedron' call with the STL triangles stored\n"
        "                      in SCAD vectors.\n"
        "\n"
        "   -f, --stl-faces    convert the input STEP file into SCAD code, retaining the\n"
        "                      'face' information from the STEP file. Each face will be rendered\n"
        "                      in a different color in openscad $preview mode.\n"
        "\n"
        "   -e, --explore      Work-in-progress code, used for development and exploration\n"
        "                      of OpenCASCADE class hierarchy, e.g.\n"
        "                      Shell->Face->Surface->Wire->Edge->Vertex.\n"
        "                      produces debug messges and no useful output.\n"
        "\n"
        "Written by Assaf Gordon (assafgordon@gmail.com)\n"
        "License: LGPLv2.1 or later\n"
        "\n";
    exit(0);
}

void show_version()
{
    std::cout << 42 << std::endl;
    exit(0);
}

// Simple Windows-compatible command line parser
OutputFormat parse_command_line(int argc, char* argv[], const Option* options, std::string& filename, double& stl_lin_tol) {
    OutputFormat output = OUT_UNDEFINED;
    stl_lin_tol = 0.5; // default linear tolerance

    // Skip program name
    int argIndex = 1;

    while (argIndex < argc) {
        std::string arg = argv[argIndex];

        // Check if it's an option (starts with - or --)
        if (arg[0] == '-') {
            // Long option
            if (arg[1] == '-') {
                std::string option_name = arg.substr(2);
                bool found = false;

                for (int i = 0; options[i].name != 0; i++) {
                    if (option_name == options[i].name) {
                        found = true;

                        // Handle option with argument
                        if (options[i].has_arg && argIndex + 1 < argc) {
                            if (options[i].val == 'L') {
                                stl_lin_tol = atof(argv[argIndex + 1]);
                                if (stl_lin_tol <= 0) {
                                    std::cerr << "Invalid tolerance value '" << argv[argIndex + 1] << "'" << std::endl;
                                    exit(1);
                                }
                                argIndex++;
                            }
                        }

                        // Handle option based on its value
                        switch (options[i].val) {
                        case 'h': show_help(); break;
                        case 'V': show_version(); break;
                        case 'a': output = OUT_STL_ASCII; break;
                        case 's': output = OUT_STL_SCAD; break;
                        case 'f': output = OUT_STL_FACES; break;
                        case 'o': output = OUT_STL_OCCT; break;
                        case 'e': output = OUT_EXPLORE; break;
                        }
                        break;
                    }
                }

                if (!found) {
                    std::cerr << "Unknown option: " << arg << std::endl;
                    exit(1);
                }
            }
            // Short option
            else {
                char option_char = arg[1];
                bool found = false;

                for (int i = 0; options[i].name != 0; i++) {
                    if (option_char == options[i].val) {
                        found = true;

                        // Handle option with argument
                        if (options[i].has_arg && argIndex + 1 < argc) {
                            if (options[i].val == 'L') {
                                stl_lin_tol = atof(argv[argIndex + 1]);
                                if (stl_lin_tol <= 0) {
                                    std::cerr << "Invalid tolerance value '" << argv[argIndex + 1] << "'" << std::endl;
                                    exit(1);
                                }
                                argIndex++;
                            }
                        }

                        // Handle option based on its value
                        switch (options[i].val) {
                        case 'h': show_help(); break;
                        case 'V': show_version(); break;
                        case 'a': output = OUT_STL_ASCII; break;
                        case 's': output = OUT_STL_SCAD; break;
                        case 'f': output = OUT_STL_FACES; break;
                        case 'o': output = OUT_STL_OCCT; break;
                        case 'e': output = OUT_EXPLORE; break;
                        }
                        break;
                    }
                }

                if (!found) {
                    std::cerr << "Unknown option: " << arg << std::endl;
                    exit(1);
                }
            }
        }
        else {
            // Not an option - should be the filename
            filename = arg;
        }

        argIndex++;
    }

    if (filename.empty()) {
        std::cerr << "Missing input STEP filename. Use --help for usage information" << std::endl;
        exit(1);
    }

    if (output == OUT_UNDEFINED) {
        std::cerr << "Missing output format option. Use --help for usage information" << std::endl;
        exit(1);
    }

    // Return the selected output format
    return output;
}

int main(int argc, char* argv[])
{
    // Setup console for UTF-8 output
    SetConsoleOutputCP(CP_UTF8);

    std::string filename;
    double stl_lin_tol;

    OutputFormat output = parse_command_line(argc, argv, options, filename, stl_lin_tol);

    /* Load the shape from STEP file.
       See https://github.com/miho/OCC-CSG/blob/master/src/occ-csg.cpp#L311
       and https://github.com/lvk88/OccTutorial/blob/master/OtherExamples/runners/convertStepToStl.cpp
     */
    TopoDS_Shape shape;

    STEPControl_Reader Reader;
    IFSelect_ReturnStatus s = Reader.ReadFile(filename.c_str());
    if (s != IFSelect_RetDone) {
        std::cerr << "Failed to load STEP file '" << filename << "'" << std::endl;
        return 1;
    }
    Reader.TransferRoots();
    shape = Reader.OneShape();

    /* Is this required (for Tessellation and/or StlAPI_Writer?) */
    BRepMesh_IncrementalMesh mesh(shape, stl_lin_tol);
    mesh.Perform();

    Face_vector faces;

    if ((output == OUT_STL_ASCII) || (output == OUT_STL_SCAD) || (output == OUT_STL_FACES))
        faces = tessellate_shape(shape);

    switch (output)
    {
    case OUT_STL_ASCII:
        write_triangles_ascii_stl(faces);
        break;

    case OUT_STL_SCAD:
        write_triangle_scad(faces);
        break;

    case OUT_STL_FACES:
        write_faces_scad(faces);
        break;

    case OUT_STL_OCCT:
        try
        {
            StlAPI_Writer writer;
            // Use standard output for Windows
            writer.Write(shape, "stdout");
        }
        catch (Standard_ConstructionError& e)
        {
            std::cerr << "Failed to write OCCT/STL: " << e.GetMessageString() << std::endl;
            return 1;
        }
        break;

    case OUT_EXPLORE:
        explore_shape(shape);
        break;
    }

    return 0;
}
