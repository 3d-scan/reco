#!/usr/bin/python
import os
import sys
import os.path as osp
import cv2
import numpy as np
import argparse as ap
from lxml import etree
import re

parser = ap.ArgumentParser(description='Traverse all .mp4 video files in the specified folder and'+
                           ' pick out frames to export as images.')
parser.add_argument("-f", "--folder", help="Folder to work in", 
                    required=False, default= ["./"])
parser.add_argument("-fn", "--frame_numbers", help="Frame numbers .npz file with frame_numbers array."+
                    " If specified, program filters frame pairs based on these numbers instead of other"+
                    " criteria.",required=False, default=None)
parser.add_argument("-v", "--videos", nargs=2, help="Input stereo video tuple (left, right)", 
                    required=False, default= ["left.mp4","right.mp4"])
parser.add_argument("-pf", "--preview_files", nargs=2, help="Input frames to test calibration result", 
                    required=False, default= ["left.png","right.png"])
parser.add_argument("-p", "--preview", help="Test calibration result on left/right frame pair", 
                    action = "store_true", required=False)
parser.add_argument("-bw", "--board_width", help="checkerboard inner corner width",required = False, 
                    default= 15)
parser.add_argument("-bh", "--board_height", help="checkerboard inner corner height",required = False,
                     default= 8)
parser.add_argument("-t", "--sharpness_threshold", help="sharpness threshold based on variance of "+
                    "Laplacian; used to filter out frames that are too blurry.", 
                    type=float, required = False, default=55.0)
parser.add_argument("-d", "--difference_threshold", help="difference threshold: maximum average "
                    +" per-pixel difference (in range [0,1.0]) between current and previous frames to "
                    +"filter out frames that are too much alike", type=float, required = False, default=0.4)
parser.add_argument("-c", "--corners_file", required = False, default="corners.npz")
parser.add_argument("-s", "--save_corners", action='store_true', required = False, default=False)
parser.add_argument("-l", "--load_corners", action='store_true', required = False, default=False)
parser.add_argument("-ds", "--precalibrate_solo", action='store_true', required = False, default=False)
parser.add_argument("-i", "--max_iterations", help="maximum number of iterations for the stereo"+
                    " calibration (optimization) loop", type=int, required = False, default=30)
parser.add_argument("-d8", "--use_8_distortion_coefficients", action='store_true', required = False, 
                    default=False)
parser.add_argument("-dt", "--use_tangential_distortion_coefficients", action='store_true', 
                    required = False, default=False)
parser.add_argument("-df", "--use_fisheye_distortion_model", action='store_true', 
                    required = False, default=False)
parser.add_argument("-sp", "--skip_printing_output", action='store_true', required = False, default= False)
parser.add_argument("-ss", "--skip_saving_output", action='store_true', required = False, default= False)
parser.add_argument("-o", "--output", help="output file to store calibration results", 
                    required = False, default="cvcalib.xml")
parser.add_argument("-u", "--use_existing", 
                    help="use the existing output file to initialize calibration parameters", 
                    action="store_true", required = False, default=False)
parser.add_argument("-ff", "--filtered_frame_folder", help="filtered frames"+
                    " will be saved into this folder (relative to work folder specified by --folder)", 
                    required = False, default="frames")
parser.add_argument("-sf", "--save_frames", action='store_true', required = False, default= False)
parser.add_argument("-lf", "--load_frames", action='store_true', required = False, default= False)
parser.add_argument("-m", "--manual_filter", help="pick which (pre-filtered)frames to use manually"+
                    " one-by-one (use 'a' key to approve)", required = False, action='store_true', 
                    default=False)


def print_output(error, K1, d1, K2, d2, R, T):
    print "Final reprojection error: " + str(error)
    print "K0:"
    print K1
    print "K1:"
    print K2
    print "d0:"
    print d1
    print "d1:"
    print d2
    print "R:"
    print R
    print "T:"
    print T
    
def opencv_matrix_xml_element(root, mat,name):
    mat_element = etree.SubElement(root, name, attrib={"type_id":"opencv-matrix"})
    rows_elem = etree.SubElement(mat_element, "rows")
    rows_elem.text = str(mat.shape[0])
    cols_elem = etree.SubElement(mat_element, "cols")
    cols_elem.text = str(mat.shape[1])
    dt_elem = etree.SubElement(mat_element, "dt")
    if(mat.dtype == np.dtype('float64')):
        dt_elem.text = "d"
    elif(mat.dtype == np.dtype("float32")):
        dt_elem.text = "f"
    else:
        raise ValueError("dtype " + str(mat.dtype) + "not supported")
    
    data_elem = etree.SubElement(mat_element, "data")
    data_string = str(mat.flatten()).replace("\n","").replace("[","").replace("]","")
    data_string = re.sub("\s+"," ",data_string)
    data_elem.text = data_string
    return mat_element
    
def save_output_opencv(error, K1, d1, K2, d2, R, T, dims, file_path):
    root = etree.Element("opencv_storage")
    width_element = etree.SubElement(root, "width")
    width_element.text = str(dims[1])
    width_element = etree.SubElement(root, "height")
    width_element.text = str(dims[0])
    K1_element = opencv_matrix_xml_element(root, K1, "K1")
    d1_element = opencv_matrix_xml_element(root, d1, "d1")
    K2_element = opencv_matrix_xml_element(root, K2, "K2")
    d2_element = opencv_matrix_xml_element(root, d2, "d2")
    R_element = opencv_matrix_xml_element(root, R, "R")
    T_element = opencv_matrix_xml_element(root, T, "T")
    error_element = etree.SubElement(root, "reprojection_error")
    error_element.text = str(error)
    et = etree.ElementTree(root)
    with open(file_path,'w') as f:
        et.write(f,encoding="utf-8",xml_declaration=True, pretty_print=True)
    s=open(file_path).read()
    s = s.replace("'","\"")
    with open(file_path,'w') as f:
        f.write(s)
        f.flush()
        
def parse_xml_matrix(mat_element):
    rows = int(mat_element.find("rows").text)
    cols = int(mat_element.find("cols").text)
    type_flag = mat_element.find("dt").text
    if type_flag == "f":
        dtype = np.float32
    elif type_flag == "d":
        dtype = np.float64
    else:
        raise ValueError("dtype flag " + type_flag + " not supported." )
    data_string = mat_element.find("data").text
    data = np.array([float(str) for str in data_string.strip().split(" ") if len(str) > 0])
    return data.reshape((rows,cols)).astype(dtype)

def generate_preview(K1, d1, K2, d2, R, T, test_im_left, test_im_right):
    R1 = np.identity(3,np.float64)
    R2 = np.identity(3,np.float64)
    P1 = np.zeros((3,4),np.float64)
    P2 = np.zeros((3,4),np.float64)
    im_size = test_im_left.shape
    new_size = (int(test_im_left.shape[0]*1.5),int(test_im_left.shape[1]*1.5))
    R1, R2, P1, P2, Q = cv2.stereoRectify(cameraMatrix1=K1, 
                            distCoeffs1=d1, 
                            cameraMatrix2=K2, 
                            distCoeffs2=d2, 
                            imageSize=im_size, 
                            R=R,T=T, flags=cv2.CALIB_ZERO_DISPARITY, 
                            newImageSize=new_size)
    map1x, map1y = cv2.initUndistortRectifyMap(K1, d1, R1, P1, new_size, cv2.CV_32FC1)
    map2x, map2y = cv2.initUndistortRectifyMap(K2, d2, R2, P2, new_size, cv2.CV_32FC1)
    
    rect_left = cv2.remap(test_im_left, map1x, map1y, cv2.INTER_LINEAR)
    rect_right = cv2.remap(test_im_right, map2x, map2y, cv2.INTER_LINEAR)
    return rect_left, rect_right
        
def load_output_opencv(path):
    tree = etree.parse(path)
    error = float(tree.find("reprojection_error").text)
    K1 = parse_xml_matrix(tree.find("K1"))
    d1 = parse_xml_matrix(tree.find("d1"))
    K2 = parse_xml_matrix(tree.find("K2"))
    d2 = parse_xml_matrix(tree.find("d2"))
    R = parse_xml_matrix(tree.find("R"))
    T = parse_xml_matrix(tree.find("T"))
    width = float(tree.find("width").text)
    height = float(tree.find("height").text)
    return error, K1, d1, K2, d2, R, T, (height,width)
    

def automatic_filter(lframe,rframe,lframe_prev,rframe_prev,sharpness_threshold, difference_threshold):
    sharpness = min(cv2.Laplacian(lframe, cv2.CV_64F).var(), cv2.Laplacian(rframe, cv2.CV_64F).var())
    #compare left frame to the previous left **filtered** one
    ldiff = np.sum(abs(lframe_prev - lframe)) / diff_div
    if sharpness < args.sharpness_threshold or ldiff < args.difference_threshold:
        return False, None, None
    
    lfound,lcorners = cv2.findChessboardCorners(lframe,board_dims)
    rfound,rcorners = cv2.findChessboardCorners(rframe,board_dims)
    if not (lfound and rfound):
        return False, None, None
    
    return True, lcorners, rcorners


if __name__ == "__main__":
    args = parser.parse_args()
    left_vid = args.folder + os.path.sep + args.videos[0]
    right_vid = args.folder + os.path.sep +  args.videos[1]
    
    print "Calibrating based on files: "
    print(left_vid)
    print(right_vid)
    
    if args.save_frames or args.load_frames:
        full_frame_folder_path = args.folder + os.path.sep + args.filtered_frame_folder
        print full_frame_folder_path
        #if filtered frame folder is specified but doesn't exist, create it
        if not os.path.exists(full_frame_folder_path):
            os.makedirs(full_frame_folder_path)
            
    if args.frame_numbers:
        path = osp.join(args.folder, args.frame_numbers)
        print "Loading frame numbers from \"" + path + "\"."
        npzfile = np.load(path)
        frame_numbers = set(npzfile["frame_numbers"])

    left_cap = cv2.VideoCapture(left_vid)
    right_cap = cv2.VideoCapture(right_vid)
    
    board_dims = (args.board_width,args.board_height)
    
    limgpoints = []
    rimgpoints = []
    objpoints = []

    objp = np.zeros((args.board_height*args.board_width,1,3), np.float32)
    objp[:,:,:2] = np.indices(board_dims).T.reshape(-1, 1, 2)
    #convert inches to meters
    objp *= 0.0508
    
    lret, lframe = left_cap.read()
    rret, rframe = right_cap.read()
    frame_dims = lframe.shape[:-1]
    
    usable_frame_ct = 0
    i_frame = 0
    criteria_subpix = (cv2.TERM_CRITERIA_EPS + cv2.TERM_CRITERIA_MAX_ITER, 30, 0.001)
    
    if(args.load_corners):
        path = args.folder + os.path.sep + args.corners_file
        print "Loading corners from " + path 
        npzfile = np.load(path)
        limgpoints = npzfile['limgpoints']
        rimgpoints = npzfile['rimgpoints']
        usable_frame_ct = len(limgpoints)
        for i_frame in xrange(usable_frame_ct):
            objpoints.append(objp)
        i_frame +=1
    elif(args.load_frames):
        files = [f for f in os.listdir(full_frame_folder_path) if osp.isfile(osp.join(full_frame_folder_path,f)) and f.endswith(".png")]
        files.sort()
        #assume matching numbers in corresponding left & right files
        rfiles = [f for f in files if "r_" in f]
        lfiles = [f for f in files if "l_" in f]
        #assume all are usable
        for ix_pair in xrange(min(len(lfiles),len(rfiles))):
            lframe = cv2.imread(osp.join(full_frame_folder_path,lfiles[ix_pair]))
            rframe = cv2.imread(osp.join(full_frame_folder_path,rfiles[ix_pair]))
            lfound,lcorners = cv2.findChessboardCorners(lframe,board_dims)
            rfound,rcorners = cv2.findChessboardCorners(rframe,board_dims)
            lgrey = cv2.cvtColor(lframe,cv2.COLOR_BGR2GRAY)
            rgrey = cv2.cvtColor(rframe,cv2.COLOR_BGR2GRAY)
            cv2.cornerSubPix(lgrey, lcorners, (11,11),(-1,-1),criteria_subpix)
            cv2.cornerSubPix(rgrey, rcorners, (11,11),(-1,-1),criteria_subpix)
            limgpoints.append(lcorners)
            rimgpoints.append(rcorners)
            objpoints.append(objp)
            usable_frame_ct+=1
            i_frame+=1
        if args.save_corners:
            path = args.folder + os.path.sep + args.corners_file
            print "Saving corners..."
            np.savez_compressed(path,limgpoints=limgpoints,rimgpoints=rimgpoints)
    else:
        #init capture
        cont_cap = lret and rret
        lframe_prev = np.zeros(lframe.shape, lframe.dtype)
        rframe_prev = np.zeros(rframe.shape, rframe.dtype)
        diff_div = lframe.shape[0] * lframe.shape[1] * 3 * 256.0
        report_interval = 10
        #for keeping track of the difference between frames
        diff = 0 
        
        
        while(cont_cap):
            sharpness = min(cv2.Laplacian(lframe, cv2.CV_64F).var(), cv2.Laplacian(rframe, cv2.CV_64F).var())
            #compare left frame to the previous left **filtered** one
            ldiff = np.sum(abs(lframe_prev - lframe)) / diff_div
            
            if not args.frame_numbers or i_frame in frame_numbers:
                add_corners, lcorners, rcorners = automatic_filter(lframe, rframe, lframe_prev, rframe_prev, 
                                                                   args.sharpness_threshold, args.difference_threshold)
                
                
                if args.manual_filter:
                    combo = np.hstack((lframe, rframe))
                    cv2.imshow("frame", combo)
                    key = cv2.waitKey(0) & 0xFF
                    add_corners = (key == ord('a'))
                    cv2.destroyWindow("frame")
                    
                if add_corners:
                    usable_frame_ct += 1
                    if(usable_frame_ct % report_interval == 0):
                        print "Usable frames: " + str(usable_frame_ct) + " (" + str(usable_frame_ct * 100.0/(i_frame+1.0)) + " %)"
                    lgrey = cv2.cvtColor(lframe,cv2.COLOR_BGR2GRAY)
                    rgrey = cv2.cvtColor(rframe,cv2.COLOR_BGR2GRAY)
                    cv2.cornerSubPix(lgrey, lcorners, (11,11),(-1,-1),criteria_subpix)
                    cv2.cornerSubPix(rgrey, rcorners, (11,11),(-1,-1),criteria_subpix)
                    
                    #save frames if filtered frame folder was specified
                    if(args.save_frames):
                        lfname =full_frame_folder_path + os.path.sep + args.videos[0][:-4] + "{0:04d}".format(i_frame) + ".png"
                        cv2.imwrite(lfname, lframe)
                        cv2.imwrite(full_frame_folder_path + os.path.sep + args.videos[1][:-4] 
                                    + "{0:04d}".format(i_frame) + ".png", rframe)
                        
                    limgpoints.append(lcorners)
                    rimgpoints.append(rcorners) 
                    objpoints.append(objp)
                
                    #log last usable **filtered** frame
                    lframe_prev = lframe
                    rframe_prev = rframe
    
            i_frame += 1
            lret, lframe = left_cap.read()
            rret, rframe = right_cap.read()
            cont_cap = lret and rret
            if args.manual_filter and key == 27:
                cont_cap = False
                
        if(args.manual_filter):
            cv2.destroyAllWindows()
            
        if args.save_corners:
            path = args.folder + os.path.sep + args.corners_file
            print "Saving corners..."
            np.savez_compressed(path,limgpoints=limgpoints,rimgpoints=rimgpoints)
        
    left_cap.release()
    right_cap.release()
    
    print "Total usable frames: " + str(usable_frame_ct) + " (" + str(usable_frame_ct * 100.0/(i_frame)) + " %)"
    print "Calibrating for max. " +str(args.max_iterations) + " iterations...."
    
    flags = 0
    
    if args.use_existing:
        prev_error, K1, d1, K2, d2, R, T, prev_dims = load_output_opencv(osp.join(args.folder, args.output))
        if(frame_dims != prev_dims):
            raise ValueError("Frame dimensions in specified calibration file (" + 
                             osp.join(args.folder, args.output) + " don't correspond to video files.")
        flags += cv2.CALIB_USE_INTRINSIC_GUESS
    else:
        K1 = np.eye(3,3,np.float64)
        K2 = np.eye(3,3,np.float64)
    
    criteria = (cv2.TERM_CRITERIA_MAX_ITER + cv2.TERM_CRITERIA_EPS, args.max_iterations, 2.2204460492503131e-16)
    
    
    if args.use_fisheye_distortion_model:
        if not args.use_existing:  
            d1 = np.zeros(4,np.float64)
            d2 = np.zeros(4,np.float64)
        obp2 = []
        for pointset in objpoints:
            obp2.append(np.transpose(pointset,(1,0,2)).astype(np.float64))
        
        lpts2 = []
        for pointset in limgpoints:
            lpts2.append(np.transpose(pointset,(1,0,2)).astype(np.float64))
        
        rpts2 = []
        for pointset in rimgpoints:
            rpts2.append(np.transpose(pointset,(1,0,2)).astype(np.float64))
        #err, K1, d1, tvecs, rvecs = cv2.fisheye.calibrate(objectPoints = obp2, imagePoints = lpts2, 
        #                                                  image_size = frame_dims, K=K1, D=d1)
        #TODO: try this with transposed matrices
        error, K1, d1, K2, d2, R, T \
            = cv2.fisheye.stereoCalibrate(obp2, lpts2, rpts2, 
                                          K1, d1, K2, d2, 
                                          imageSize=frame_dims,
                                          flags=flags,
                                          criteria=criteria)
    else:
        if not args.use_existing:
            d1 = np.zeros(8,np.float64)
            d2 = np.zeros(8,np.float64)
        if not args.use_tangential_distortion_coefficients:
            flags += cv2.CALIB_ZERO_TANGENT_DIST
        if args.use_8_distortion_coefficients:
            flags += cv2.CALIB_RATIONAL_MODEL
        
        if(args.precalibrate_solo):
            err1, K1, d1, rvecs, tvecs = cv2.calibrateCamera(objpoints, limgpoints, 
                                                       frame_dims, K1, d1, flags=flags,criteria = criteria)
            print "K1 solo:"
            print K1
            print "d1 solo:"
            print d1
            print "solo 1 err:"
            print err1
            err2, K2, d2, rvecs, tvecs = cv2.calibrateCamera(objpoints, rimgpoints, 
                                                       frame_dims, K2, d2, flags=flags,criteria = criteria)
            print "K2 solo:"
            print K2
            print "d2 solo:"
            print d2
            print "solo 2 err:"
            print err2
            flags += cv2.CALIB_FIX_INTRINSIC
            #flags += cv2.CALIB_USE_INTRINSIC_GUESS
            if(int(cv2.__version__ [0]) == 2):
                error, K1_2, d1, K2_2, d2, R, T, E, F \
                    = cv2.stereoCalibrate(objpoints,limgpoints,rimgpoints,  
                                          imageSize = frame_dims,
                                          cameraMatrix1 = K1, distCoeffs1 = d1, 
                                          cameraMatrix2 = K2, distCoeffs2 = d2,
                                          flags = flags,
                                          criteria = criteria)
            else:
                error, K1, d1, K2, d2, R, T, E, F \
                        = cv2.stereoCalibrate(objpoints,limgpoints,rimgpoints, 
                                              cameraMatrix1 = K1, distCoeffs1 = d1, 
                                              cameraMatrix2 = K2, distCoeffs2 =  d2,
                                              imageSize = frame_dims,
                                              flags = flags,
                                              criteria = criteria)
        else:
            if(int(cv2.__version__ [0]) == 2):
                error, K1, d1, K2, d2, R, T, E, F \
                      = cv2.stereoCalibrate(objpoints,limgpoints,rimgpoints, 
                                            imageSize = frame_dims,
                                            flags = flags,
                                            criteria = criteria)
            else:  
                error, K1, d1, K2, d2, R, T, E, F \
                    = cv2.stereoCalibrate(objpoints,limgpoints,rimgpoints, 
                                          K1, d1, K2, d2, 
                                          imageSize = frame_dims,
                                          flags = flags,
                                          criteria = criteria)
            
    if not args.skip_printing_output:
        print_output(error, K1, d1, K2, d2, R, T)
    if not args.skip_saving_output:
        save_output_opencv(error, K1, d1, K2, d2, R, T, frame_dims, osp.join(args.folder,args.output))
        
    if args.preview:
        l_im = cv2.imread(osp.join(args.folder,args.preview_files[0]))
        r_im = cv2.imread(osp.join(args.folder,args.preview_files[1]))
        l_im, r_im = generate_preview(K1, d1, K2, d2, R, T, l_im, r_im)
        path_l = osp.join(args.folder,args.preview_files[0][:-4] + "_rect.png")
        path_r = osp.join(args.folder,args.preview_files[1][:-4] + "_rect.png")
        cv2.imwrite(path_l, l_im)
        cv2.imwrite(path_r, r_im)