/*
  curl -s 'https://api.buienradar.nl/image/1.0/radarmapbe?width=550' | \
  convert -[4] buienradar.png
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <SDL2/SDL.h>
#include <curl/curl.h>
#include <wand/magick_wand.h>

#include "buienradar.h"

const uint32_t TARGET_FRAME_RATE = 30;
const uint32_t TICKS_PER_FRAME = 1000 / TARGET_FRAME_RATE;
const uint32_t UPDATE_INTERVAL_IN_SECONDS = 300; // Update every 5 minutes.

global_variable bool Running = true;
global_variable BitMap bitmap = {0};

// Callback function used by curl when it fetches data
static size_t
WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  MemoryStruct *mem = (MemoryStruct *)userp;

  mem->memory = (char *)realloc(mem->memory, mem->size + realsize + 1);
  if (mem->memory == NULL)
  {
    /* out of memory! */
    printf("not enough memory (realloc returned NULL)\n");
    return 0;
  }

  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;

  return realsize;
}

// SDL Event loop callback
internal void
HandleEvent(SDL_Event *Event)
{
  switch (Event->type)
  {
  case SDL_QUIT:
  {
    Running = false;
  }
  break;

  case SDL_KEYDOWN:
  {
    if (Event->key.keysym.sym == SDLK_ESCAPE)
    {
      Running = false;
    }
  }
  break;
  }
}

// Convert GIF to BMP with GraphicsMagick
void
ConvertToBitmap(MemoryStruct *chunk)
{
  MagickWand *magick_wand;
  MagickPassFail status = MagickPass;
  InitializeMagick(0);
  magick_wand = NewMagickWand();

  if (status == MagickPass)
  {
    status = MagickReadImageBlob(magick_wand,
                                 (const unsigned char *)chunk->memory,
                                 chunk->size);
  }

  if (status == MagickPass)
  {
    printf("MagicWand succes!\n");
    MagickSetImageFormat(magick_wand, "BMP");

    free(bitmap.data);
    bitmap.data = MagickWriteImageBlob(magick_wand, &bitmap.size);
    printf("BmpImageSize: %lu\n", bitmap.size);
  }
  else
  {
    printf("MagicWand error!\n");
  }

  DestroyMagickWand(magick_wand);

  // Destroy GraphicsMagick API
  DestroyMagick();
}

// Update the map using curl
void
UpdateMap()
{
  CURL *curl_handle;
  CURLcode res;

  MemoryStruct chunk;

  chunk.memory = (char *)malloc(1); /* will be grown as needed by the realloc above */
  chunk.size = 0;           /* no data at this point */

  curl_global_init(CURL_GLOBAL_ALL);

  /* init the curl session */
  curl_handle = curl_easy_init();

  /* specify URL to get */
  curl_easy_setopt(curl_handle, CURLOPT_URL, "https://api.buienradar.nl/image/1.0/radarmapbe?width=550");

  /* send all data to this function  */
  curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);

  /* we pass our 'chunk' struct to the callback function */
  curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);

  /* some servers don't like requests that are made without a user-agent
     field, so we provide one */
  curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

  /* get it! */
  res = curl_easy_perform(curl_handle);

  /* check for errors */
  if (res != CURLE_OK)
  {
    fprintf(stderr, "curl_easy_perform() failed: %s\n",
            curl_easy_strerror(res));
  }
  else
  {
    /*
    * Now, our chunk.memory points to a memory block that is chunk.size
    * bytes big and contains the remote file.
    *
    * Do something nice with it!
    */

    printf("%lu bytes retrieved\n", (unsigned long)chunk.size);
    // Convert gif to bitmap
    ConvertToBitmap(&chunk);
  }

  /* cleanup curl stuff */
  curl_easy_cleanup(curl_handle);

  free(chunk.memory);

  /* we're done with libcurl, so clean it up */
  curl_global_cleanup();
}

int
main(void)
{
  UpdateMap();

  // Variables required to calculate framerate
  #define FPS_INTERVAL 1.0
  uint32_t LastFrameEndTime = SDL_GetTicks();

  SDL_DisplayMode CurrentDisplay;

  // Initialize SDL
  if (SDL_Init(SDL_INIT_VIDEO) == 0)
  {
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2");
    SDL_GetCurrentDisplayMode(0, &CurrentDisplay);

    SDL_Window *Window = NULL;
    SDL_Renderer *Renderer = NULL;

    Window = SDL_CreateWindow("Buienradar",
                              SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED,
                              0,
                              0,
                              SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (Window)
    {
      Renderer = SDL_CreateRenderer(Window,
                                    -1,
                                    SDL_RENDERER_ACCELERATED);
      if (Renderer)
      {
        SDL_RWops *ImageData = SDL_RWFromConstMem(bitmap.data,
                                                  bitmap.size);
        SDL_Surface *ImageSurface = SDL_LoadBMP_RW(ImageData, 1);        
        SDL_Texture *Image =
            SDL_CreateTextureFromSurface(Renderer,
                                         ImageSurface);
        uint32_t ScaledImageWidth = (float)CurrentDisplay.h /
                                    (float)ImageSurface->h *
                                    (float)ImageSurface->w;
        SDL_Rect TargetRect;
        TargetRect.x = (CurrentDisplay.w / 2) - (ScaledImageWidth / 2);
        TargetRect.y = 0;
        TargetRect.w = ScaledImageWidth;
        TargetRect.h = CurrentDisplay.h;

        uint32_t timer = 0;
        while (Running)
        {
          if (timer >= TARGET_FRAME_RATE * UPDATE_INTERVAL_IN_SECONDS)
          {
            // Reset timer
            timer = 0;

            printf("Update map.\n");
            // Clear relevant SDL resources
            SDL_DestroyTexture(Image);
            SDL_FreeSurface(ImageSurface);
            UpdateMap();
            ImageData = SDL_RWFromConstMem(bitmap.data,
                                           bitmap.size);
            ImageSurface = SDL_LoadBMP_RW(ImageData, 1);
            
            Image = SDL_CreateTextureFromSurface(Renderer,
                                                 ImageSurface);
            ScaledImageWidth = (float)CurrentDisplay.h /
                               (float)ImageSurface->h *
                               (float)ImageSurface->w;
            TargetRect.x = (CurrentDisplay.w / 2) - (ScaledImageWidth / 2);
            TargetRect.y = 0;
            TargetRect.w = ScaledImageWidth;
            TargetRect.h = CurrentDisplay.h;
          }
          else
          {
            timer++;
          }

          uint32_t FrameStartTime = SDL_GetTicks();
          SDL_Event Event;

          while (SDL_PollEvent(&Event))
          {
            HandleEvent(&Event);
          }

          SDL_RenderClear(Renderer);

          SDL_RenderCopy(Renderer, Image, 0, &TargetRect);

          SDL_RenderPresent(Renderer);

          // Calculate and cap framerate
          uint32_t FrameEndTime = SDL_GetTicks();

          // Calculate framerate
          if (LastFrameEndTime < FrameEndTime - FPS_INTERVAL * 1000)
          {
            LastFrameEndTime = FrameEndTime;
          }

          // Cap framerate
          uint32_t FrameTicks = FrameEndTime - FrameStartTime;

          if (FrameTicks < TICKS_PER_FRAME)
          {
            // Wait remaining time
            SDL_Delay(TICKS_PER_FRAME - FrameTicks);
          }
        }
        SDL_FreeSurface(ImageSurface);
        SDL_DestroyTexture(Image);
        SDL_DestroyRenderer(Renderer);
      }
      else {
        printf("Renderer could not be created! SDL_Error: %s\n",
                SDL_GetError());
      }
      SDL_DestroyWindow(Window);
    }
    else
    {
      printf("Window could not be created! SDL_Error: %s\n",
             SDL_GetError());
    }
  }
  else
  {
    printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
  }

  free(bitmap.data);

  SDL_Quit();

  return 0;
}
