#include <stdio.h>
#include <iostream>
#include <time.h>
#include <string>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#define THRESH_MIN 35
#define SIDE_PAD 30
#define TOP_PAD 20

// Returns a box that bounds all non-zero pixels in an image.
CvRect getBoundingBox(cv::Mat* img) {
  int width = img->cols;
  int height = img->rows;
  printf("%d x %d\n", width, height);
  int left = width;
  int right = 0;
  int top = height;
  int bottom = 0;

  for (int x = 0; x < width; x++) {
    for (int y = 0; y < height; y++) {
      if (img->at<uchar>(y, x) > 0) {
        if (left > x) left = x;
        if (right < x) right = x;
        if (top > y) top = y;
        if (bottom < y) bottom = y;
      }
    }
  }
  
  printf("left:%d\ntop:%d\nright:%d\nbottom:%d\n", left, top, right, bottom);
  return cvRect(left-SIDE_PAD, top-TOP_PAD, right-left+2*SIDE_PAD, bottom-top+2*TOP_PAD);
}

void produceAnnotation(std::string directory,
				       std::string filename,
				       cv::Mat* img,
				       std::string label,
				       CvRect bb) {
  std::string annoteDir = "annotations/";
  std::string annoteFilename = filename.substr(0, filename.length() - 4) + ".xml";
  printf("%s\n", annoteFilename.c_str());
  FILE* file = fopen((annoteDir + annoteFilename).c_str(), "w");
  assert(file);

  int tl_x, tl_y, tr_x, tr_y, bl_x, bl_y, br_x, br_y;
  tl_x = bb.x;
  tl_y = bb.y;
  tr_x = bb.x + bb.width;
  tr_y = bb.y;
  bl_x = bb.x;
  bl_y = bb.y + bb.height;
  br_x = bb.x + bb.width;
  br_y = bb.y + bb.height;

  fprintf(file,
  		  "<annotation><filename>%s</filename><folder>%s</folder><imagesize><nrows>%d</nrows><ncols>%d</ncols></imagesize><object><name>%s</name><deleted>0</deleted><polygon><pt><x>%d</x><y>%d</y></pt><pt><x>%d</x><y>%d</y></pt><pt><x>%d</x><y>%d</y></pt><pt><x>%d</x><y>%d</y></pt></polygon></object></annotation>",
  		  filename.c_str(), directory.c_str(), img->rows, img->cols, label.c_str(),
  		  tl_x, tl_y, tr_x, tr_y, br_x, br_y, bl_x, bl_y);
  fclose(file);
}

int main(int argc, char** argv)
{
  puts("Start...");
  char key;
  int i, j, k, m;
  int numImg = atoi(argv[1]);
  int numShoots = atoi(argv[2]);
  int numDiff = (numImg / numShoots) - 1;

  srand(time(NULL));

  std::string imgdir = "../imgdata/";
  std::string cropimgdir = "../cropimgdata/";
  std::string imgext = ".jpg";

  cv::namedWindow("diff", CV_WINDOW_NORMAL);
  cv::namedWindow("img1", CV_WINDOW_NORMAL);
  cv::namedWindow("dilate", CV_WINDOW_NORMAL);

  // Construct image filenames
  puts("Constructing filenames..");
  std::string imgname[numImg];
  for (i = 0; i < numImg; i++) {
    char c[3];
    sprintf(c, "%d", i+1);
    imgname[i] = std::string(c);
    if (imgname[i].length() == 1) {
      imgname[i] = "dataset-0000" + imgname[i];
    }
    else if (imgname[i].length() == 2) {
      imgname[i] = "dataset-000" + imgname[i];
    }
    imgname[i] = imgname[i] + imgext;
    puts(imgname[i].c_str());
  }

  // Load images
  puts("Loading images..");
  cv::Mat img[numImg];
  cv::Mat imgcrop[numImg];
  for (i = 0; i < numImg; i++) {
    img[i] = cv::imread(imgdir + imgname[i], CV_LOAD_IMAGE_GRAYSCALE);
  }

  // Create differential images from consecutive images
  puts("Creating differential images..");
  cv::Mat diff[numDiff*numShoots];
  k = 0;
  for (j = 0; j < numShoots; j++) {
    for (i = 0; i < numDiff; i++) {
      m = j*(numDiff+1);
      diff[k] = cv::abs(img[m+i] - img[m+i+1]);
      k++;
    }
  }

  printf("k = %d\n", k);

  // Erode/Dilate differential images
  puts("Eroding and dilating images..");
  cv::Mat result[numDiff*numShoots];
  for (i = 0; i < numDiff*numShoots; i++) {
    cv::Mat thresImg, erodeImg, dilateImg;
    cv::threshold(diff[i], thresImg, THRESH_MIN, 255, CV_THRESH_BINARY);
    puts("threshold");
    cv::erode(thresImg, erodeImg, cv::Mat());
    cv::erode(erodeImg, erodeImg, cv::Mat());
    //puts("");
    cv::erode(erodeImg, erodeImg, cv::Mat());
    cv::erode(erodeImg, erodeImg, cv::Mat());
    cv::erode(erodeImg, erodeImg, cv::Mat());
    cv::dilate(erodeImg, dilateImg, cv::Mat());
    cv::dilate(dilateImg, dilateImg, cv::Mat());
    cv::dilate(dilateImg, dilateImg, cv::Mat());
    cv::dilate(dilateImg, result[i], cv::Mat());
    //cv::imshow("dilate", result[i]);
    //cvWaitKey(0);
  }

  // Sum groups of differential images for each photoshoot group
  puts("Summing differential images..");
  cv::Mat diffSum[numShoots];
  for (j = 0; j < numShoots; j++) {
    m = j*numDiff;
    printf("m=%d\n", m);
    diffSum[j] = result[m];
    for (i = 1; i < numDiff; i++) {
      printf("index=%d\n", m+i);
      diffSum[j] = diffSum[j] + result[m+i];
    }
  }

  // Show diff sums
  for (i = 0; i < numShoots; i++) {
    cv::imshow("img1", diffSum[i]);
    cvWaitKey(0);
  }

  // Erode/Dilate summed differential images and find bounding box
  puts("Finding bounding boxes..");
  CvRect bb[numShoots];
  for (i = 0; i < numShoots; i++) {
    cv::Mat erodeImg, dilateImg;
    cv::erode(diffSum[i], erodeImg, cv::Mat());
    cv::erode(erodeImg, erodeImg, cv::Mat());
    cv::erode(erodeImg, erodeImg, cv::Mat());
    cv::erode(erodeImg, erodeImg, cv::Mat());
    cv::erode(erodeImg, erodeImg, cv::Mat());
    cv::dilate(erodeImg, dilateImg, cv::Mat());
    cv::dilate(dilateImg, dilateImg, cv::Mat());
    cv::dilate(dilateImg, dilateImg, cv::Mat());
    cv::dilate(dilateImg, dilateImg, cv::Mat());
    cv::dilate(dilateImg, dilateImg, cv::Mat());
    cv::dilate(dilateImg, dilateImg, cv::Mat());

    // find bounding rectangle
    bb[i] = getBoundingBox(&dilateImg);
  }

  CvRect offset_bb[numImg];
  // Add random offsets to bounding boxes
  k = 0;
  for (j = 0; j < numShoots; j++) {
    for (i = 0; i <= numDiff; i++) {
      int neg;
      if (rand() % 2 == 0)
        neg = -1;
      else
        neg = 1;

      int hOffset = neg*(rand() % 20 + 1); // random number between 1 and 5
      printf("offset: %d\n", hOffset);
      if (rand() % 2 == 0)
        neg = -1;
      else
        neg = 1;
      int vOffset = neg*(rand() % 20 + 1); // random number between 1 and 5

      offset_bb[k].x = bb[j].x + hOffset;
      offset_bb[k].y = bb[j].y + vOffset;
      offset_bb[k].width = bb[j].width;
      offset_bb[k].height = bb[j].height;
      k++;
    }
  }
  // Produce the cropped image which are used as positive training samples 
  for (j = 0; j < numShoots;  j++){
     for (i = 0; i <= numDiff; i ++){
        printf("producing the %dth cropped image...\n", j*numShoots+i);
        //std::cout << bb[j].x << " " << bb[j].y << " " << bb[j].width << " " << bb[j].height << std::endl;
        bb[j].x = (bb[j].x >= 0 ? bb[j].x:0);
        bb[j].y = (bb[j].y >= 0 ? bb[j].y:0);
        bb[i].width = ((bb[i].x + bb[i].width) >= img[j*numShoots+i].cols ? (img[j*numShoots+i].cols-bb[i].x):bb[i].width);
        bb[i].height = ((bb[i].y + bb[i].height) >= img[j*numShoots+i].rows ? (img[j*numShoots+i].rows-bb[i].y):bb[i].height);
        imgcrop[j*numShoots+i] = img[j*numShoots+i](bb[j]).clone();
        std::string saveimgdir = cropimgdir + imgname[i] + "crop.jpg";
        cv::imwrite(saveimgdir, imgcrop[j*numShoots+i]);
     }   
  }


  // Produce annotation files and draw bounding boxes on images
  puts("Producing annotations..");
  std::string label = "chatterbox";
  cv::Mat test[numImg];
  k = 0;
  for (j = 0; j < numShoots; j++) {
    m = j*numDiff;
    for (i = 0; i <= numDiff; i++) {
      //char c[10];
      //sprintf(c, "%d", i+1);
      //std::string filename = std::string(c) + ".jpg";
      cvtColor(img[k], test[k], CV_GRAY2BGR);
      //produceAnnotation(imgdir, imgname[k], &img[k], label, bb[j]);
      //rectangle(test[k], bb[j], cv::Scalar(0, 0, 255), 5);
      produceAnnotation(imgdir, imgname[k], &img[k], label, offset_bb[k]);
      rectangle(test[k], offset_bb[k], cv::Scalar(0, 0, 255), 5);
      k++;
    }
  }

  //cv::imshow("diff", diffSum);
  //cv::imshow("dilate", dilateImg);
  puts("Showing images..");
  for (i = 0; i < numImg; i++) {
    cv::imshow("img1", test[i]);
    cvWaitKey(0);
  }

  cv::destroyWindow("diff");
  cv::destroyWindow("img1");
  cv::destroyWindow("dilate");
  return 0;
}
