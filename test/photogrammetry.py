import os
import cv2
import numpy as np
import argparse
from tqdm import tqdm


def cropping(image, size):
    '''
    This function crops the image to a smaller size, such that the photogrammetry target takes up a larger portion of the total image.
    '''

    rgb_img = np.fromfile(image, dtype=np.uint8).reshape((3008, 4128, 3))

    img = cv2.cvtColor(rgb_img, cv2.COLOR_RGB2BGR)
    

    h, w = img.shape[:2]

    if size[0] > w or size[1] > h:
        raise ValueError("Crop dimensions exceed the image size.")

    center_x, center_y = w // 2, h // 2

    x1 = max(center_x - size[0] // 2, 0)
    y1 = max(center_y - size[1] // 2, 0)

    cropped = img[y1:y1 + size[1], x1:x1 + size[0]]

    return cropped

    

if __name__ == "__main__":
   parser = argparse.ArgumentParser(
           description="Crop images for photogrammetry preparation"
   )
   parser.add_argument("--input_folder", type=str, help="Path to folder with .raw images")
   parser.add_argument("--size", type=int, default=(1000, 1000), nargs="+", help="How many internal squares are in the checkerboard: [horizontal] [vertical]")

   args = parser.parse_args()

   size = tuple(args.size)

   input_folder = args.input_folder
   if not input_folder:
       base_dir = "/home/sst/data/alvium_test"
       subfolders= [f.path for f in os.scandir(base_dir) if f.is_dir()]
       input_folder = max(subfolders, key=os.path.getmtime)
       print(f"Automatically loading latest folder: {input_folder}")
       if not input_folder:
           raise FileNotFoundError("No available folders found through automatic search. Please specify a folder.")

   if not os.path.exists(input_folder):
       raise FileNotFoundError(f"{input_folder} does not exist.")

   output_folder = input_folder + "/cropped_images"

   if not os.path.exists(output_folder):
       os.makedirs(output_folder)

   print(f"Cropping .raw images and saving .pngs to {output_folder}")
   for filename in tqdm(sorted(os.listdir(input_folder))):
       if not filename.lower().endswith(".raw"):
           continue

       filepath = os.path.join(input_folder, filename)

       try:
           cropped = cropping(filepath, size)
               
       except Exception as e:
           print(f"Error cropping {filename}: {e}")
           continue

       outpath = os.path.join(output_folder, os.path.splitext(filename)[0] + ".png")
       cv2.imwrite(outpath, cropped)



