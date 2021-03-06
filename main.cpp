#include <opencv2/objdetect/objdetect.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <iostream>
#include <queue>
#include <stdio.h>
#include <math.h>
#include <ctime>


//include string library
#include <string>
#include <sstream>

#include "constants.h"
#include "findEyeCenter.h"
#include "findEyeCorner.h"

/* Attempt at supporting openCV version 4.0.1 or higher */
#if CV_MAJOR_VERSION >= 4
#define CV_WINDOW_NORMAL                cv::WINDOW_NORMAL
#define CV_BGR2YCrCb                    cv::COLOR_BGR2YCrCb
#define CV_HAAR_SCALE_IMAGE             cv::CASCADE_SCALE_IMAGE
#define CV_HAAR_FIND_BIGGEST_OBJECT     cv::CASCADE_FIND_BIGGEST_OBJECT
#endif


/** Constants **/
// Calibration sensitivity
//const double SENSITIVITY = 1.0;

/** Function Headers */
void detectAndDisplay( cv::Mat frame );

/** Global variables */
//-- Note, either copy these two files from opencv/data/haarscascades to your current folder, or change these locations
cv::String face_cascade_name = "frontal_face_features.xml";
cv::CascadeClassifier face_cascade;
std::string main_window_name = "Capture - Face detection";
std::string face_window_name = "Capture - Face";
std::string callibration_window = "Calibration";
cv::RNG rng(12345);
cv::Mat debugImage;
cv::Mat skinCrCbHist = cv::Mat::zeros(cv::Size(256, 256), CV_8UC1);
cv::Mat calibration_screen; //= cv::Mat(1200, 1500, CV_8UC3, cv::Scalar::all(255));
// Timer for calibration, create it and start it
std::clock_t start = std::clock();
// set the callibration point
cv::Point calibration_point;
cv::Point calibration_point_standard;
double left_pupil_standard_x;
double left_pupil_standard_y;
double left_pupil_x;
double left_pupil_y;
// set % calibration points
double calibration_point_x;
double calibration_point_y;
double calibration_point_x_standard;
double calibration_point_y_standard;
// set average calibration points counter
int calibration_counter = 1;

int set = true;

// Duration variable
double duration;


/**
 * @function main
 */
int main( int argc, const char** argv ) {
  cv::Mat frame;

  // Load the cascades
  if( !face_cascade.load( face_cascade_name ) ){ printf("--(!)Error loading face cascade, please change face_cascade_name in source code.\n"); return -1; };
  cv::namedWindow(main_window_name, CV_WINDOW_NORMAL);
  cv::moveWindow(main_window_name, 400, 100);
  cv::namedWindow(face_window_name, cv::WINDOW_AUTOSIZE);
  cv::moveWindow(face_window_name, 10, 100);
  cv::namedWindow("Right Eye", CV_WINDOW_NORMAL);
  cv::moveWindow("Right Eye", 10, 600);
  cv::namedWindow("Left Eye", CV_WINDOW_NORMAL);
  cv::moveWindow("Left Eye", 10, 800);

  


  cv::namedWindow(callibration_window, cv::WINDOW_AUTOSIZE);
  cv::moveWindow(callibration_window, 800, 100);


  /* As the matrix dichotomy will not be applied, these windows are useless.
  cv::namedWindow("aa",CV_WINDOW_NORMAL);
  cv::moveWindow("aa", 10, 800);
  cv::namedWindow("aaa",CV_WINDOW_NORMAL);
  cv::moveWindow("aaa", 10, 800);*/

  createCornerKernels();
  ellipse(skinCrCbHist, cv::Point(113, 155), cv::Size(23, 15),
          43.0, 0.0, 360.0, cv::Scalar(255, 255, 255), -1);

  // I make an attempt at supporting both 2.x and 3.x OpenCV
#if CV_MAJOR_VERSION < 3
  CvCapture* capture = cvCaptureFromCAM( 0 );
  if( capture ) {
    while( true ) {
      frame = cvQueryFrame( capture );
#else
  cv::VideoCapture capture(0);
  if( capture.isOpened() ) {
    while( true ) {
      capture.read(frame);
#endif
      // mirror it
      cv::flip(frame, frame, 1);
      frame.copyTo(debugImage);

      // Apply the classifier to the frame
      if( !frame.empty() ) {
        detectAndDisplay( frame );
      }
      else {
        printf(" --(!) No captured frame -- Break!");
        break;
      }

      imshow(main_window_name,debugImage);
      imshow(callibration_window, calibration_screen);
      int c = cv::waitKey(10);
      if( (char)c == 'c' ) { break; }
      if( (char)c == 'f' ) {
        imwrite("frame.png",frame);
      }

    }
  }

  releaseCornerKernels();

  return 0;
}

std::string return_pupil_coordinates(cv::Point leftPupil, cv::Point rightPupil) {
    std::ostringstream o;
    o << "Left Pupil:" << leftPupil.x << "."  << leftPupil.y << ", Right Pupil: " << rightPupil.x << "." << rightPupil.y;
    return o.str();
}

void findEyes(cv::Mat frame_gray, cv::Rect face) {
  cv::Mat faceROI = frame_gray(face);
  cv::Mat debugFace = faceROI;
  calibration_screen = cv::Mat(1200, 1500, CV_8UC3, cv::Scalar::all(255));
  if (kSmoothFaceImage) {
    double sigma = kSmoothFaceFactor * face.width;
    GaussianBlur( faceROI, faceROI, cv::Size( 0, 0 ), sigma);
  }
  //-- Find eye regions and draw them
  int eye_region_width = face.width * (kEyePercentWidth/100.0);
  int eye_region_height = face.width * (kEyePercentHeight/100.0);
  int eye_region_top = face.height * (kEyePercentTop/100.0);
  cv::Rect leftEyeRegion(face.width*(kEyePercentSide/100.0),
                         eye_region_top,eye_region_width,eye_region_height);
  cv::Rect rightEyeRegion(face.width - eye_region_width - face.width*(kEyePercentSide/100.0),
                          eye_region_top,eye_region_width,eye_region_height);

  //-- Find Eye Centers
  cv::Point leftPupil = findEyeCenter(faceROI,leftEyeRegion,"Left Eye");
  cv::Point rightPupil = findEyeCenter(faceROI,rightEyeRegion,"Right Eye");
  // get corner regions
  cv::Rect leftRightCornerRegion(leftEyeRegion);
  leftRightCornerRegion.width -= leftPupil.x;
  leftRightCornerRegion.x += leftPupil.x;
  leftRightCornerRegion.height /= 2;
  leftRightCornerRegion.y += leftRightCornerRegion.height / 2;
  cv::Rect leftLeftCornerRegion(leftEyeRegion);
  leftLeftCornerRegion.width = leftPupil.x;
  leftLeftCornerRegion.height /= 2;
  leftLeftCornerRegion.y += leftLeftCornerRegion.height / 2;
  cv::Rect rightLeftCornerRegion(rightEyeRegion);
  rightLeftCornerRegion.width = rightPupil.x;
  rightLeftCornerRegion.height /= 2;
  rightLeftCornerRegion.y += rightLeftCornerRegion.height / 2;
  cv::Rect rightRightCornerRegion(rightEyeRegion);
  rightRightCornerRegion.width -= rightPupil.x;
  rightRightCornerRegion.x += rightPupil.x;
  rightRightCornerRegion.height /= 2;
  rightRightCornerRegion.y += rightRightCornerRegion.height / 2;
  rectangle(debugFace,leftRightCornerRegion,200);
  rectangle(debugFace,leftLeftCornerRegion,200);
  rectangle(debugFace,rightLeftCornerRegion,200);
  rectangle(debugFace,rightRightCornerRegion,200);
  // change eye centers to face coordinates
  rightPupil.x += rightEyeRegion.x;
  rightPupil.y += rightEyeRegion.y;
  leftPupil.x += leftEyeRegion.x;
  leftPupil.y += leftEyeRegion.y;
  // draw eye centers
  circle(debugFace, rightPupil, 3, 1234);
  circle(debugFace, leftPupil, 3, 1234);
  circle(debugFace, rightPupil, 10, 1234);
  circle(debugFace, leftPupil, 10, 1234);

  const double SENSITIVITY = 1.0;
  duration = (std::clock() - start) / (double)CLOCKS_PER_SEC;

  // Get the middle
  if (duration < 5)
  {
      calibration_point.x = calibration_screen.size().width / 2;
      calibration_point.y = calibration_screen.size().height / 2;

      //calibration_point_standard.x = leftPupil.x;
      //calibration_point_standard.y = leftPupil.y;
      calibration_point_x_standard = (double)leftPupil.x / debugFace.size().width;
      calibration_point_y_standard = (double)leftPupil.y / debugFace.size().height;
      //left_pupil_standard_x = (double)leftPupil.x / calibration_counter;
      left_pupil_standard_x = (double)leftPupil.x;
      // calibration_point_standard.x = leftPupil.x;
       //calibration_point_standard.y = leftPupil.y;
      printf("LeftPupil x : %f\n", left_pupil_standard_x);
      printf("Size width : %d\n", calibration_screen.size().width);
      printf("Calibration point x standard: %f\n", calibration_point_x_standard);
      calibration_counter++;

  }
  else
  {
      //printf("Calibration point.x : %d\n", calibration_point.x);
      //printf("Calibration point.y : %d\n", calibration_point.y);
      ///calibration_point.x = ( ( (calibration_screen.size().width / 2)  ) / leftPupil.x ) * calibration_point_standard.x;
      //calibration_point.y = ( ( (calibration_screen.size().height / 2) ) / leftPupil.y ) * calibration_point_standard.y;
      calibration_point_x = (double)leftPupil.x / debugFace.size().width;
      printf("Calibration point x: %f\n", calibration_point_x);
      printf("Calibration circle x : %d\n", calibration_point.x);
      if (calibration_point_x_standard - calibration_point_x < -0.1)
      {
          //calibration_point.x = (int)((double)calibration_screen.size().width * 0.5 * calibration_point_x  / calibration_point_x_standard);
          calibration_point.x = (int)((double)(calibration_screen.size().width * 0.5 * calibration_point_x / calibration_point_x_standard) + eye_region_width*2.5);
      }
      else if (calibration_point_x_standard - calibration_point_x > 0.1)
      {
          //calibration_point.x = (int)((double)calibration_screen.size().width * 0.5 * calibration_point_x  / calibration_point_x_standard);
          calibration_point.x = (int)((double)(calibration_screen.size().width * 0.5 * calibration_point_x / calibration_point_x_standard) - eye_region_width*2.5);
      }
      else
      {
          //calibration_point.x = (int)((double)calibration_screen.size().width * 1.0 * calibration_point_x);
          calibration_point.x = calibration_screen.size().width / 2;
      }
  }
  
  circle(calibration_screen, calibration_point, 10, 1234);
  //printf("Right Pupil: %d . %d \n", rightPupil.x, rightPupil.y);
  //printf("Left Pupil: %d . %d \n", leftPupil.x, leftPupil.y);
  cv::putText(debugFace, //target image
      return_pupil_coordinates(leftPupil, rightPupil),//text
      cv::Point(10, 30),
      cv::FONT_HERSHEY_DUPLEX,
      0.35,
      CV_RGB(255, 255, 255), //font color
      0.1);

  //-- Find Eye Corners - ERRORS
  if (kEnableEyeCorner) {
    cv::Point2f leftRightCorner = findEyeCorner(faceROI(leftRightCornerRegion), true, false);
    leftRightCorner.x += leftRightCornerRegion.x;
    leftRightCorner.y += leftRightCornerRegion.y;
    cv::Point2f leftLeftCorner = findEyeCorner(faceROI(leftLeftCornerRegion), true, true);
    leftLeftCorner.x += leftLeftCornerRegion.x;
    leftLeftCorner.y += leftLeftCornerRegion.y;
    cv::Point2f rightLeftCorner = findEyeCorner(faceROI(rightLeftCornerRegion), false, true);
    rightLeftCorner.x += rightLeftCornerRegion.x;
    rightLeftCorner.y += rightLeftCornerRegion.y;
    cv::Point2f rightRightCorner = findEyeCorner(faceROI(rightRightCornerRegion), false, false);
    rightRightCorner.x += rightRightCornerRegion.x;
    rightRightCorner.y += rightRightCornerRegion.y;
    circle(faceROI, leftRightCorner, 3, 200);
    circle(faceROI, leftLeftCorner, 3, 200);
    circle(faceROI, rightLeftCorner, 3, 200);
    circle(faceROI, rightRightCorner, 3, 200);
  }

  imshow(face_window_name, faceROI);
//  cv::Rect roi( cv::Point( 0, 0 ), faceROI.size());
//  cv::Mat destinationROI = debugImage( roi );
//  faceROI.copyTo( destinationROI );
}


cv::Mat findSkin (cv::Mat &frame) {
  cv::Mat input;
  cv::Mat output = cv::Mat(frame.rows,frame.cols, CV_8U);

  cvtColor(frame, input, CV_BGR2YCrCb);

  for (int y = 0; y < input.rows; ++y) {
    const cv::Vec3b *Mr = input.ptr<cv::Vec3b>(y);
//    uchar *Or = output.ptr<uchar>(y);
    cv::Vec3b *Or = frame.ptr<cv::Vec3b>(y);
    for (int x = 0; x < input.cols; ++x) {
      cv::Vec3b ycrcb = Mr[x];
//      Or[x] = (skinCrCbHist.at<uchar>(ycrcb[1], ycrcb[2]) > 0) ? 255 : 0;
      if(skinCrCbHist.at<uchar>(ycrcb[1], ycrcb[2]) == 0) {
        Or[x] = cv::Vec3b(0,0,0);
      }
    }
  }
  return output;
}

/**
 * @function detectAndDisplay
 */
void detectAndDisplay( cv::Mat frame ) {
  std::vector<cv::Rect> faces;
  //cv::Mat frame_gray;

  std::vector<cv::Mat> rgbChannels(3);
  cv::split(frame, rgbChannels);
  cv::Mat frame_gray = rgbChannels[2];

  //cvtColor( frame, frame_gray, CV_BGR2GRAY );
  //equalizeHist( frame_gray, frame_gray );
  //cv::pow(frame_gray, CV_64F, frame_gray);
  //-- Detect faces
  face_cascade.detectMultiScale( frame_gray, faces, 1.1, 2, 0|CV_HAAR_SCALE_IMAGE|CV_HAAR_FIND_BIGGEST_OBJECT, cv::Size(150, 150) );
//  findSkin(debugImage);

  for( int i = 0; i < faces.size(); i++ )
  {
    rectangle(debugImage, faces[i], 1234);
  }
  //-- Show what you got
  if (faces.size() > 0) {
    findEyes(frame_gray, faces[0]);
  }
}