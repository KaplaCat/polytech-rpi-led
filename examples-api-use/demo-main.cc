#include "led-matrix.h"
#include "threaded-canvas-manipulator.h"
#include "pixel-mapper.h"
#include "graphics.h"

#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>

using std::min;
using std::max;

#define TERM_ERR  "\033[1;31m"
#define TERM_NORM "\033[0m"

using namespace rgb_matrix;

volatile bool interrupt_received = false;
static void InterruptHandler(int signo) {
  interrupt_received = true;
}

// -------------------- CLASS ANIMATION --------------------- //
class ImageScroller : public ThreadedCanvasManipulator {
public:
  // Scroll image with "scroll_jumps" pixels every "scroll_ms" milliseconds.
  // If "scroll_ms" is negative, don't do any scrolling.
  ImageScroller(RGBMatrix *m, int scroll_jumps, int scroll_ms = 30)
    : ThreadedCanvasManipulator(m), scroll_jumps_(scroll_jumps),
      scroll_ms_(scroll_ms),
      horizontal_position_(0),
      matrix_(m) {
    offscreen_ = matrix_->CreateFrameCanvas();
  }

  virtual ~ImageScroller() {
    Stop();
    WaitStopped();   // only now it is safe to delete our instance variables.
  }

  // _very_ simplified. Can only read binary P6 PPM. Expects newlines in headers
  // Not really robust. Use at your own risk :)
  // This allows reload of an image while things are running, e.g. you can
  // life-update the content.
  bool LoadPPM(const char *filename) {
    FILE *f = fopen(filename, "r");
    // check if file exists
    if (f == NULL && access(filename, F_OK) == -1) {
      fprintf(stderr, "File \"%s\" doesn't exist\n", filename);
      return false;
    }
    if (f == NULL) return false;
    char header_buf[256];
    const char *line = ReadLine(f, header_buf, sizeof(header_buf));
#define EXIT_WITH_MSG(m) { fprintf(stderr, "%s: %s |%s", filename, m, line); \
      fclose(f); return false; }
    if (sscanf(line, "P6 ") == EOF)
      EXIT_WITH_MSG("Can only handle P6 as PPM type.");
    line = ReadLine(f, header_buf, sizeof(header_buf));
    int new_width, new_height;
    if (!line || sscanf(line, "%d %d ", &new_width, &new_height) != 2)
      EXIT_WITH_MSG("Width/height expected");
    int value;
    line = ReadLine(f, header_buf, sizeof(header_buf));
    if (!line || sscanf(line, "%d ", &value) != 1 || value != 255)
      EXIT_WITH_MSG("Only 255 for maxval allowed.");
    const size_t pixel_count = new_width * new_height;
    Pixel *new_image = new Pixel [ pixel_count ];
    assert(sizeof(Pixel) == 3);   // we make that assumption.
    if (fread(new_image, sizeof(Pixel), pixel_count, f) != pixel_count) {
      line = "";
      EXIT_WITH_MSG("Not enough pixels read.");
    }
#undef EXIT_WITH_MSG
    fclose(f);
    //fprintf(stderr, "Read image '%s' with %dx%d\n", filename,
    //        new_width, new_height);
    horizontal_position_ = 0;
    MutexLock l(&mutex_new_image_);
    new_image_.Delete();  // in case we reload faster than is picked up
    new_image_.image = new_image;
    new_image_.width = new_width;
    new_image_.height = new_height;
    return true;
  }

  void Run() {
    const int screen_height = offscreen_->height();
    const int screen_width = offscreen_->width();
    while (running() && !interrupt_received) {
      {
        MutexLock l(&mutex_new_image_);
        if (new_image_.IsValid()) {
          current_image_.Delete();
          current_image_ = new_image_;
          new_image_.Reset();
        }
      }
      if (!current_image_.IsValid()) {
        usleep(100 * 1000);
        continue;
      }
      for (int x = 0; x < screen_width; ++x) {
        for (int y = 0; y < screen_height; ++y) {
          const Pixel &p = current_image_.getPixel(
            (horizontal_position_ + x) % current_image_.width, y);
          offscreen_->SetPixel(x, y, p.red, p.green, p.blue);
        }
      }		
		usleep(scroll_ms_ * 1000);
 
      offscreen_ = matrix_->SwapOnVSync(offscreen_);
      /*horizontal_position_ += scroll_jumps_;
      if (horizontal_position_ < 0) horizontal_position_ = current_image_.width;
      if (scroll_ms_ <= 0) {
        // No scrolling. We don't need the image anymore.
        current_image_.Delete();
      } else {
        usleep(scroll_ms_ * 1000);
      }*/
    }
  }

private:
  struct Pixel {
    Pixel() : red(0), green(0), blue(0){}
    uint8_t red;
    uint8_t green;
    uint8_t blue;
  };

  struct Image {
    Image() : width(-1), height(-1), image(NULL) {}
    ~Image() { Delete(); }
    void Delete() { delete [] image; Reset(); }
    void Reset() { image = NULL; width = -1; height = -1; }
    inline bool IsValid() { return image && height > 0 && width > 0; }
    const Pixel &getPixel(int x, int y) {
      static Pixel black;
      if (x < 0 || x >= width || y < 0 || y >= height) return black;
      return image[x + width * y];
    }

    int width;
    int height;
    Pixel *image;
  };

  // Read line, skip comments.
  char *ReadLine(FILE *f, char *buffer, size_t len) {
    char *result;
    do {
      result = fgets(buffer, len, f);
    } while (result != NULL && result[0] == '#');
    return result;
  }

  const int scroll_jumps_;
  const int scroll_ms_;

  // Current image is only manipulated in our thread.
  Image current_image_;

  // New image can be loaded from another thread, then taken over in main thread
  Mutex mutex_new_image_;
  Image new_image_;

  int32_t horizontal_position_;

  RGBMatrix* matrix_;
  FrameCanvas* offscreen_;
};

// -------------------- MAIN --------------------- //
int main(int argc, char *argv[]) 
{
    RGBMatrix::Options matrix_options;
    rgb_matrix::RuntimeOptions runtime_opt;
  
    int runtime_seconds = 10;
	const char *demo_parameter_1 = "pika-test-1.ppm";
	const char *demo_parameter_2 = "pika-test-2.ppm";
	const char *demo_parameter_3 = "pika-test-3.ppm";
	const char *demo_parameter_4 = "pika-test-4.ppm";
	int scroll_ms = 300;
	int demo = 1;

	
	matrix_options.cols = 64;
    matrix_options.rows = 32;
    matrix_options.chain_length = 1;
    matrix_options.parallel = 1;

    RGBMatrix *matrix = CreateMatrixFromOptions(matrix_options, runtime_opt);
    if (matrix == NULL)
      return 1;

    Canvas *canvas = matrix;
    ThreadedCanvasManipulator *image_gen_1 = NULL;
	ThreadedCanvasManipulator *image_gen_2 = NULL;
	ThreadedCanvasManipulator *image_gen_3 = NULL;
	ThreadedCanvasManipulator *image_gen_4 = NULL;
	
	for(int i=1;i>0;i++) //INFINI
	//for(int i=0;i<10;i++)	//10x
	{
		if (demo_parameter_1) {
		  ImageScroller *scroller = new ImageScroller(matrix,
													  demo,
													  scroll_ms);
		  if (!scroller->LoadPPM(demo_parameter_1))
			return 1;
		  image_gen_1 = scroller;
		} else {
		  fprintf(stderr, "Demo %d Requires PPM image as parameter\n", demo);
		  return 1;
		}

		signal(SIGTERM, InterruptHandler);
		signal(SIGINT, InterruptHandler);

		image_gen_1->Start();
		
		usleep(runtime_seconds * 1000);
		delete image_gen_1;

		
		runtime_seconds = 10;
		if (demo_parameter_2) {
		  ImageScroller *scroller = new ImageScroller(matrix,
													  demo,
													  scroll_ms);
		  if (!scroller->LoadPPM(demo_parameter_2))
			return 1;
		  image_gen_2 = scroller;
		} else {
		  fprintf(stderr, "Demo %d Requires PPM image as parameter\n", demo);
		  return 1;
		}

		signal(SIGTERM, InterruptHandler);
		signal(SIGINT, InterruptHandler);
		
		image_gen_1->Stop();
		image_gen_2->Start();

		usleep(runtime_seconds * 1000);
		delete image_gen_2;

		
		runtime_seconds = 10;

		if (demo_parameter_3) {
		  ImageScroller *scroller = new ImageScroller(matrix,
													  demo,
													  scroll_ms);
		  if (!scroller->LoadPPM(demo_parameter_3))
			return 1;
		  image_gen_3 = scroller;
		} else {
		  fprintf(stderr, "Demo %d Requires PPM image as parameter\n", demo);
		  return 1;
		}

		signal(SIGTERM, InterruptHandler);
		signal(SIGINT, InterruptHandler);

		image_gen_2->Stop();
		image_gen_3->Start();

		usleep(runtime_seconds * 1000);
		delete image_gen_3;

		
		runtime_seconds = 10;
		
		if (demo_parameter_4) {
		  ImageScroller *scroller = new ImageScroller(matrix,
													  demo,
													  scroll_ms);
		  if (!scroller->LoadPPM(demo_parameter_4))
			return 1;
		  image_gen_4 = scroller;
		} else {
		  fprintf(stderr, "Demo %d Requires PPM image as parameter\n", demo);
		  return 1;
		}

		signal(SIGTERM, InterruptHandler);
		signal(SIGINT, InterruptHandler);

		image_gen_3->Stop();
		image_gen_4->Start();
		usleep(runtime_seconds * 1000);
		delete image_gen_4;
	
		
	}

}
