// haimage.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <filesystem>
#include <fstream>
#include <vector>
#include <memory>
#include <string>
#include "image.h"


struct rbyte
{
	unsigned char rest;
	unsigned char top;
};



enum eModes {
	MODE_EXPORT = 1,
};



int main(int argc, char* argv[])
{
	if (argc == 1) {
		std::cout << "HAImage - a tool to extract Harvester animated image files by ermaccer (.ABM)\n"
			<< "Output images are sometimes glitched, you can fix that manually.\n"
			<< "Usage: himage <params> <file>\n"
			<< "    -e              Extract\n"
			<< "    -o <folder>     Sets output folder\n"
			<< "    -p              Specifies PAL file\n";

		return 1;
	}

	int mode = 0;
	std::string o_param;
	std::string p_param;
	// params
	for (int i = 1; i < argc - 1; i++)
	{
		if (argv[i][0] != '-' || strlen(argv[i]) != 2) {
			return 1;
		}
		switch (argv[i][1])
		{
		case 'e': mode = MODE_EXPORT;
			break;
		case 'o':
			i++;
			o_param = argv[i];
			break;
		case 'p':
			i++;
			p_param = argv[i];
			break;
		default:
			std::cout << "ERROR: Param does not exist: " << argv[i] << std::endl;
			break;
		}
	}


	if (mode == MODE_EXPORT)
	{
		std::ifstream pFile(argv[argc - 1], std::ifstream::binary);

		if (!pFile)
		{
			std::cout << "ERROR: Could not open: " << argv[argc - 1] << "!" << std::endl;
			return 1;
		}

		if (p_param.empty())
		{
			std::cout << "ERROR: Palette not specified!" << std::endl;
			return 1;
		}

		std::ifstream palFile(p_param, std::ifstream::binary);
		if (!palFile)
		{
			std::cout << "ERROR: Could not open: " << p_param.c_str() << "!" << std::endl;
			return 1;
		}


		if (!o_param.empty())
		{
			if (!std::filesystem::exists(o_param))
				std::filesystem::create_directory(o_param);
			std::filesystem::current_path(o_param);
		}



		// 256 colors
		std::unique_ptr<rgb_pal_entry[]> palData = std::make_unique<rgb_pal_entry[]>(256);
		std::unique_ptr<rgbr_pal_entry[]> palData4 = std::make_unique<rgbr_pal_entry[]>(256);

		for (int i = 0; i < 256; i++)
			palFile.read((char*)&palData[i], sizeof(rgb_pal_entry));

		for (int i = 0; i < 256; i++)
		{
			palData4[i].r = palData[i].r;
			palData4[i].g = palData[i].g;
			palData4[i].b = palData[i].b;
			palData4[i].reserved = 0x00;
		}



		if (pFile)
		{
			int entries;
			int _pad;

			pFile.read((char*)&entries, sizeof(int));
			pFile.read((char*)&_pad, sizeof(int));


			for (int i = 0; i < entries; i++)
			{
				harvester_abm abm;
				pFile.read((char*)&abm, sizeof(harvester_abm));

				std::cout << "Processing image " << i + 1 << "/" << entries << std::endl;

				if (abm.flag)
				{
					std::vector<unsigned char> dataBuff;
					std::unique_ptr<unsigned char[]> compressedBuff = std::make_unique<unsigned char[]>(abm.size);
					pFile.read((char*)compressedBuff.get(), abm.size);
					// decompress
					for (int a = 0; a < abm.size;)
					{
						unsigned char chr = compressedBuff[a++];
						rbyte byte;
						byte.top = chr & 0x80;
						byte.rest = chr & 0x7F;

						if (byte.top == 0)
						{
							for (int x = 0; x < byte.rest; x++)
							{
								unsigned char tmp = compressedBuff[a++];
								dataBuff.push_back(tmp);
							}
						}
						else
						{
							unsigned char tmp = compressedBuff[a++];
							for (int x = 0; x < byte.rest; x++)
							{
								dataBuff.push_back(tmp);

							}

						}

					}

					// create bmp
					bmp_header bmp;
					bmp_info_header bmpf;
					bmp.bfType = 'MB';
					bmp.bfSize = abm.width * abm.height;
					bmp.bfReserved1 = 0;
					bmp.bfReserved2 = 0;
					bmp.bfOffBits = sizeof(bmp_header) + sizeof(bmp_info_header) + 1024;
					bmpf.biSize = sizeof(bmp_info_header);
					bmpf.biWidth = abm.width;
					bmpf.biHeight = abm.height;
					bmpf.biPlanes = 1;
					bmpf.biBitCount = 8;
					bmpf.biCompression = 0;
					bmpf.biXPelsPerMeter = 2835;
					bmpf.biYPelsPerMeter = 2835;
					bmpf.biClrUsed = 256;
					bmpf.biClrImportant = 0;

					std::string output = argv[argc - 1];
					output.insert(0, std::to_string(i));
					output += ".bmp";

					std::ofstream oFile(output, std::ofstream::binary);

					oFile.write((char*)&bmp, sizeof(bmp_header));
					oFile.write((char*)&bmpf, sizeof(bmp_info_header));
					// swap red and blue 
		
					for (int a = 0; a < 256; a++)
					{
						char r = palData4[a].r;
						char b = palData4[a].b;
						palData4[a].r = b;
						palData4[a].b = r;
						oFile.write((char*)&palData4[a], sizeof(rgbr_pal_entry));
					}
			
					int size = abm.width;

					// from msdn
					if (abm.width & 0x1)
						size = ((((abm.width * 8) + 31) & ~31) >> 3);

					std::unique_ptr<unsigned char[]> imageBuff = std::make_unique<unsigned char[]>(size * abm.height);

					for (int y = 0; y < abm.height; y++)
					{
						int padRowSize = (y * size);
						int rowSize = (y * abm.width);
						unsigned char *dest = &imageBuff[padRowSize];
						unsigned char *src = &dataBuff[rowSize];

						memcpy(dest, src, abm.width);
					}


					for (int a = 0; a < size * abm.height; a++)
						oFile.write((char*)&imageBuff[size * abm.height - a], sizeof(char));
				}
				else
				{
					std::cout << "Image " << i << " not supported!" << std::endl;
					pFile.seekg(abm.size, pFile.cur);
				}
			}

		}
	}
}


