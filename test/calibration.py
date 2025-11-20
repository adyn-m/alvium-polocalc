import os
import cv2
import numpy as np
import argparse
from vmbpy import vmbsystem
from vmbpy.c_binding.vmb_image_transform import VmbImage, VmbImageInfo, VmbPixelInfo, VmbPixelFormat, VmbTransformInfo, VmbTransformType, call_vmb_image_transform
import ctypes
from tqdm import tqdm

def calibration_parameters(image_folder, args, show_corners=False):
    '''
    Calibrate camera from a folder of images.

    Args:
        image_folder (str): folder with images
        pattern_size (tuple): number of squares in checkerboard pattern
        square_size (float): size of each square
        show_corners (bool): draw chessboard corners

    Returns: 
        camera_matrix (np.ndarray): Matrix encapsulating sensor resolution and camera focal length.
        dist_coeffs (np.ndarray): Distortion coefficients for the camera
        rms_error (float): RMS error for calibration computations
    '''

    pattern_size = tuple(args.pattern_size)
    square_size = args.square_size
    preprocessing = args.preprocessing

    objp = np.zeros((pattern_size[0]*pattern_size[1], 3), np.float32)
    objp[:, :2] = np.mgrid[0:pattern_size[0], 0:pattern_size[1]].T.reshape(-1, 2)
    objp *= square_size

    objpoints, imgpoints = [], []
    
    images = sorted([
        os.path.join(image_folder, f)
        for f in os.listdir(image_folder)
        if f.lower().endswith((".png"))
    ])

    gray_shape = None

    print("Beginning calibration loop")
    i = 0
    for filepath in tqdm(images):
        
        if preprocessing:
            processed = calibration_preprocess(filepath, args)
        else:
            img = cv2.imread(filepath)
            if img is None:
                print(f"Skipping {filepath}")
                continue
            processed = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
        
        processed_shape = processed.shape[::-1]

        # Find chessboard corners
        ret, corners = cv2.findChessboardCorners(processed, pattern_size, None)
        # print("Chessboard corners complete")
        if ret:
            corners_sub = cv2.cornerSubPix(
                    processed, corners, (11, 11), (-1, -1),
                    criteria=(cv2.TERM_CRITERIA_EPS + cv2.TERM_CRITERIA_MAX_ITER, 30, 0.001))
            imgpoints.append(corners_sub)
            objpoints.append(objp)

            if show_corners:
                cv2.drawChessboardCorners(img, pattern_size, corners_sub, ret)
                cv2.imshow("Corners", img)
                cv2.waitKey(0)

    if show_corners:
        cv2.destroyAllWindows()

    if not objpoints:
        raise RuntimeError("No chessboard corners detected!")

    rms_error, camera_matrix, dist_coeffs, rvecs, tvecs = cv2.calibrateCamera(objpoints, imgpoints, processed_shape, None, None)

    print(f"Calibration RMS error: {rms_error}")
    print(f"Camera matrix:\n{camera_matrix}")
    print(f"Distortion coefficients:\n{dist_coeffs}")

    return camera_matrix, dist_coeffs, rms_error


def process_raw_opencv(file_path, width=4128, height=3008): 
    rgb_img = np.fromfile(file_path, dtype=np.uint8).reshape((height, width, 3))

    bgr_img = cv2.cvtColor(rgb_img, cv2.COLOR_RGB2BGR)

    return bgr_img

def process_grayscale(file_path, width=4128, height=3008):
    img = np.fromfile(file_path, dtype=np.uint8).reshape((height, width, 3))
    gray_img = cv2.cvtColor(img, cv2.COLOR_RGB2GRAY)

    return gray_img

def calibration_preprocess(filepath, args):

    img = cv2.imread(filepath)
    # Grayscale conversion
    gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
    
    # Denoising
    gray = cv2.fastNlMeansDenoising(gray, h=7, templateWindowSize=7, searchWindowSize=21)

    # CLAHE equalization
    clahe = cv2.createCLAHE(clipLimit=2.0, tileGridSize=(8,8))
    gray = clahe.apply(gray)

    # Mild sharpening
    blur = cv2.GaussianBlur(gray, (0, 0), sigmaX=1.0)
    sharpened = cv2.addWeighted(gray, 1.5, blur, -0.5, 0)

    # Upsampling
    h, w = sharpened.shape
    sharpened = cv2.resize(sharpened, (int(w*1.5), int(h*1.5)), interpolation=cv2.INTER_CUBIC)

    sharpened_folder = args.input_folder + "/sharpened_images"

    if not os.path.exists(sharpened_folder):
        os.makedirs(sharpened_folder)

    outpath_sharpened = os.path.join(sharpened_folder, os.path.basename(filepath))
    cv2.imwrite(outpath_sharpened, sharpened)
	
    # print("Image pre-processed successfully.")
    return sharpened



def main():

    parser = argparse.ArgumentParser(
            description="Convert raw Bayer images to color PNGs and run camera calibration."
    )
    parser.add_argument("--input_folder", type=str, help="Path to folder with .raw images")
    parser.add_argument("--width", type=int, default=4128, help="Image width in pixels")
    parser.add_argument("--height", type=int, default=3008, help="Image height in pixels")
    parser.add_argument("--pattern_size", type=int, default=(10, 7), nargs="+", help="How many internal squares are in the checkerboard [horizontal] [vertical]")
    parser.add_argument("--square_size", type=float, default=0.02176, help="Square size in metres.")
    parser.add_argument("--mode", type=str, default="processing", help="calib to determine calibration parameters, focus to determine best focus based on a series of images.")
    parser.add_argument("--preprocessing", type=bool, default=False, help="denoising, equalization, and upsampling the checkerboard.")

    args = parser.parse_args()

    input_folder = args.input_folder
    if not input_folder:
        base_dir = "/home/sst/data/alvium_test"
        subfolders = [f.path for f in os.scandir(base_dir) if f.is_dir()]
        input_folder = max(subfolders, key=os.path.getmtime)
        print(f"Automatically loading latest folder: {input_folder}")
        args.input_folder = input_folder
        if not input_folder:
            raise FileNotFoundError("No available folders found through automatic search. Please specify a folder.")
    

    if not os.path.exists(input_folder):
        raise FileNotFoundError(f"{input_folder} does not exist.")


    output_folder = input_folder + "/processed_images"
    mode = args.mode
    width = args.width
    height = args.height

    if not os.path.exists(output_folder):
        os.makedirs(output_folder)

    print(f"Processing .raw images and saving .pngs to {output_folder}")
    for filename in tqdm(sorted(os.listdir(input_folder))):
        if not filename.lower().endswith(".raw"):
            continue

        output_file = os.path.join(output_folder, os.path.splitext(filename)[0] + ".png")

        if not os.path.exists(output_file):
            filepath = os.path.join(input_folder, filename)

            try:
                processed_color = process_raw_opencv(filepath, width, height)


            except Exception as e:
                print(f"Error reading {filename}: {e}")
                continue


            outpath = os.path.join(output_folder, os.path.splitext(filename)[0] + ".png")
            cv2.imwrite(outpath, processed_color)

    if mode == "calib":
        print("Images processed")
        camera_matrix, dist_coeffs, rms_error = calibration_parameters(output_folder, args)
    elif mode == "focus":
        best_focus_path, focus_scores = best_focus(output_folder)
            

if __name__ == "__main__":
    main()
