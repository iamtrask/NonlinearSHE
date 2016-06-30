#include <iostream>
#include <vector>
#include <algorithm>

#include <CImg.h>

#include "../source/yashe.hpp"
#include "../source/cipherText.hpp"
#include "../source/functions.hpp"
#include "imageFunctions.hpp"

/**
 * This demonstrates homomorphic operations being done
 * on images. An input image is encrypted pixelwise and
 * homomorphically. Then a homomorphic transformation 
 * is done on it. In this case we convert the RGB values 
 * of the pixels of the image to HSV (Hue, Saturation, Value).
 * Then we rotate the hue by a constant amount, morphing
 * the color of the image. The result is then decrypted
 * and displayed next to the original.
 *
 * Due to the constraints of homomorphic computation the
 * image is scaled down to be of size 100 by 100 pixels.
 * Then we perform batch homomorphic computation on a 
 * vector of pixels of size ~10,000.
 */
int main() {
  // Our image input
  cimg_library::CImg<unsigned char> img("../resources/test2.jpg");

  clock_t start, end;

  start = clock();

  // Generate parameters for the YASHE protocol
  // and create environment.
  long t = 257;
  NTL::ZZ q = NTL::GenPrime_ZZ(1600);
  long d = 66048; // 2^9*3*43 - 1075 irreducible factors
  long sigma = 8;
  NTL::ZZ w = NTL::power2_ZZ(300);
  YASHE SHE(t,q,d,sigma,w);

  end = clock();
  std::cout << "Factorization completed in "
            << double(end - start)/CLOCKS_PER_SEC
            << " seconds"
            << std::endl;

  // Resize the image so we can pack it into a single
  // cipher text
  long imgDim = sqrt(SHE.getNumFactors());
  img.resize(imgDim,imgDim, 1, 3, 5);

  // Define a color space of 256 colors
  cimg_library::CImg<unsigned char> colorMap =
    cimg_library::CImg<unsigned char>::default_LUT256();

  // Convert the image into a list of integers
  // each integer represents a pixel defined
  // by the color mapping above. 
  std::vector<long> message;
  ImageFunctions::imageToLongs(message, img, colorMap);

  // Resize the message so that it fills the entire
  // message space (the end of it will be junk)
  message.resize(SHE.getNumFactors());

  // Define a function on pixels.
  // This function takes a pixel value (0 - 255)
  // and returns another pixel value representing
  // the inversion of that pixel value.
  std::function<long(long)> invertColors = [colorMap](long input) {
    unsigned char r, g, b;
    double h, s, v;
    ImageFunctions::longToRGB(input, r, g, b, colorMap);
    ImageFunctions::RGBtoHSV(r, g, b, &h, &s, &v);

    // rotate hue by 30 degrees
    h = fmod(h + 30, 360);

    ImageFunctions::HSVtoRGB(&r, &g, &b, h, s, v);
    ImageFunctions::RGBToLong(input, r, g, b, colorMap);

    return input;
  };
  // The function is converted into
  // a polynomial of degree t = 257
  std::vector<long> poly = Functions::functionToPoly(invertColors, t);

  start = clock();

  // Generate public, secret and evaluation keys
  NTL::ZZ_pX secretKey = SHE.keyGen();

  end = clock();
  std::cout << "Key generation completed in "
            << double(end - start)/CLOCKS_PER_SEC
            << " seconds"
            << std::endl;
  start = clock();

  // encrypt the message
  YASHE_CT ciphertext = SHE.encryptBatch(message);

  end = clock();
  std::cout << "Encryption completed in "
            << double(end - start)/CLOCKS_PER_SEC
            << " seconds"
            << std::endl;
  start = clock();

  // evaluate the polynomial
  YASHE_CT::evalPoly(ciphertext, ciphertext, poly);

  end = clock();
  std::cout << "Polynomial evaluation completed in "
            << double(end - start)/CLOCKS_PER_SEC
            << " seconds"
            << std::endl;
  start = clock();

  // decrypt the message
  std::vector<long> decryption = SHE.decryptBatch(ciphertext, secretKey);

  end = clock();
  std::cout << "Decryption completed in "
            << double(end - start)/CLOCKS_PER_SEC
            << " seconds"
            << std::endl;

  // turn the message back into an image
  cimg_library::CImg<unsigned char> outputImg(imgDim, imgDim, 1, 3);
  ImageFunctions::longsToImage(decryption, outputImg, colorMap);

  // Display the input next to the output!
  (img, outputImg).display("result!");

  return 0;
}