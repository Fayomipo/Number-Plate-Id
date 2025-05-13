#!/usr/bin/env python3

import cv2
import imutils
import numpy as np
import pytesseract
import re
import time
import sys
import logging
from threading import Thread
from queue import Queue

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

# Add LoRa library path
sys.path.insert(0, '/home/pi/LoRa')
try:
    from SX127x.LoRa import *
    from SX127x.board_config import BOARD
except ImportError:
    logger.error("Failed to import LoRa modules. Make sure they are installed correctly.")
    sys.exit(1)

class LoRaTransceiver(LoRa):
    """Class for handling LoRa communication"""
    
    def __init__(self, verbose=False):
        super(LoRaTransceiver, self).__init__(verbose)
        self.set_mode(MODE.SLEEP)
        self.set_dio_mapping([0] * 6)
        self._init_lora_config()
        
    def _init_lora_config(self):
        """Configure LoRa parameters"""
        try:
            self.set_pa_config(pa_select=1)
            self.set_freq(915.0)
            self.set_spreading_factor(7)
            self.set_bw(BW.BW125)
            self.set_coding_rate(CODING_RATE.CR4_5)
            self.set_preamble(8)
            self.set_sync_word(0x12)
            self.set_rx_crc(True)
            logger.info("LoRa transceiver configured successfully")
        except Exception as e:
            logger.error(f"Error configuring LoRa: {e}")
            raise

    def on_rx_done(self):
        """Handle received data callback"""
        logger.info("RxDone")
        self.clear_irq_flags(RxDone=1)
        payload = self.read_payload(nocheck=True)
        message = bytes(payload).decode("utf-8", 'ignore')
        logger.info(f"Received: {message}")
        self.set_mode(MODE.SLEEP)
        self.reset_ptr_rx()
        self.set_mode(MODE.RXCONT)

    def send_data(self, data):
        """Send data over LoRa"""
        if not data:
            logger.warning("Attempted to send empty data")
            return False
            
        try:
            encoded_data = data.encode()
            self.write_payload(list(encoded_data))
            self.set_mode(MODE.TX)
            
            # Wait for transmission to complete with timeout
            start_time = time.time()
            while (self.get_irq_flags()['tx_done'] == 0):
                if time.time() - start_time > 5:  # 5-second timeout
                    logger.error("Transmission timeout")
                    return False
                time.sleep(0.01)
                
            self.clear_irq_flags(TxDone=1)
            logger.info(f"Data sent: {data}")
            return True
        except Exception as e:
            logger.error(f"Error sending data: {e}")
            return False


def preprocess_image(frame):
    """
    Process the image to extract license plate
    
    Args:
        frame: Camera frame to process
        
    Returns:
        Processed license plate image or None if no plate found
    """
    try:
        # Resize the frame for faster processing
        frame = imutils.resize(frame, width=640)
        
        # Convert to grayscale
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        
        # Apply bilateral filter to remove noise while keeping edges sharp
        gray = cv2.bilateralFilter(gray, 11, 17, 17)
        
        # Find edges
        edged = cv2.Canny(gray, 30, 200)
        
        # Find contours
        cnts = cv2.findContours(edged.copy(), cv2.RETR_TREE, cv2.CHAIN_APPROX_SIMPLE)
        cnts = imutils.grab_contours(cnts)
        
        # Sort contours by area (largest first) and take the top 10
        cnts = sorted(cnts, key=cv2.contourArea, reverse=True)[:10]
        
        license_plate_contour = None
        
        # Loop over contours to find rectangles (potential license plates)
        for c in cnts:
            peri = cv2.arcLength(c, True)
            approx = cv2.approxPolyDP(c, 0.02 * peri, True)
            
            # If our approximated contour has four points, it's likely a license plate
            if len(approx) == 4:
                license_plate_contour = approx
                break
        
        if license_plate_contour is None:
            return None
            
        # Draw contour on the original frame
        cv2.drawContours(frame, [license_plate_contour], -1, (0, 255, 0), 3)
        
        # Mask and extract the license plate
        mask = np.zeros(gray.shape, np.uint8)
        cv2.drawContours(mask, [license_plate_contour], 0, 255, -1)
        license_plate = cv2.bitwise_and(gray, gray, mask=mask)
        
        # Threshold the license plate
        _, license_plate = cv2.threshold(license_plate, 0, 255, cv2.THRESH_BINARY | cv2.THRESH_OTSU)
        
        # Remove noise
        kernel = np.ones((3, 3), np.uint8)
        license_plate = cv2.morphologyEx(license_plate, cv2.MORPH_OPEN, kernel)
        
        return license_plate
        
    except Exception as e:
        logger.error(f"Error preprocessing image: {e}")
        return None


def validate_license_plate(text):
    """
    Validate if text matches Nigerian license plate format
    
    Args:
        text: OCR text to validate
        
    Returns:
        Validated plate number or None
    """
    if not text:
        return None
        
    # Clean the text
    cleaned_text = text.strip()
    
    # Nigerian license plate pattern: AAA-123AA
    pattern = r'^[A-Z]{3}-\d{3}[A-Z]{2}$'
    
    if re.match(pattern, cleaned_text):
        return cleaned_text
    
    return None


class PlateDetectionThread(Thread):
    """Thread for processing video frames"""
    
    def __init__(self, frame_queue, result_queue):
        """Initialize thread with input and output queues"""
        Thread.__init__(self)
        self.daemon = True
        self.frame_queue = frame_queue
        self.result_queue = result_queue
        self.running = True
        
    def run(self):
        """Process frames from the queue"""
        while self.running:
            try:
                if self.frame_queue.empty():
                    time.sleep(0.01)
                    continue
                    
                frame = self.frame_queue.get()
                
                # Process the frame
                license_plate = preprocess_image(frame)
                
                if license_plate is not None:
                    # Perform OCR with optimized parameters
                    text = pytesseract.image_to_string(
                        license_plate, 
                        lang='eng',
                        config='--psm 7 --oem 3 -c tessedit_char_whitelist=ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-'
                    )
                    
                    plate_number = validate_license_plate(text)
                    
                    if plate_number:
                        self.result_queue.put(plate_number)
                
                self.frame_queue.task_done()
                
            except Exception as e:
                logger.error(f"Error in processing thread: {e}")
                
    def stop(self):
        """Stop the thread"""
        self.running = False


def main():
    """Main function"""
    # Initialize LoRa board
    logger.info("Setting up LoRa board...")
    BOARD.setup()
    
    # Create LoRa instance
    try:
        lora = LoRaTransceiver(verbose=False)
    except Exception as e:
        logger.error(f"Failed to initialize LoRa: {e}")
        BOARD.teardown()
        return
    
    # Initialize video capture
    logger.info("Initializing camera...")
    cap = cv2.VideoCapture(0)
    
    if not cap.isOpened():
        logger.error("Failed to open camera")
        lora.set_mode(MODE.SLEEP)
        BOARD.teardown()
        return
    
    # Set camera parameters
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 1280)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)
    cap.set(cv2.CAP_PROP_FPS, 30)
    
    # Create queues for thread communication
    frame_queue = Queue(maxsize=10)
    result_queue = Queue()
    
    # Create and start processing thread
    processing_thread = PlateDetectionThread(frame_queue, result_queue)
    processing_thread.start()
    
    # Set of already sent license plates (to avoid duplicate transmissions)
    sent_plates = set()
    last_sent_time = 0
    
    try:
        logger.info("Starting license plate detection...")
        while True:
            ret, frame = cap.read()
            if not ret:
                logger.warning("Failed to read from camera")
                time.sleep(1)
                continue
            
            # Add frame to processing queue if not full
            if not frame_queue.full():
                frame_queue.put(frame.copy())
            
            # Check for detected license plates
            while not result_queue.empty():
                plate_number = result_queue.get()
                
                # Avoid sending duplicate plates too frequently
                current_time = time.time()
                if (plate_number not in sent_plates or 
                    current_time - last_sent_time > 10):  # 10 seconds between resends
                    
                    logger.info(f"Nigerian License Plate detected: {plate_number}")
                    
                    # Send via LoRa
                    if lora.send_data(plate_number):
                        sent_plates.add(plate_number)
                        
                        # Limit the size of sent_plates set
                        if len(sent_plates) > 100:
                            sent_plates.pop()
                            
                        last_sent_time = current_time
            
            # Display video feed
            cv2.putText(frame, "Press 'q' to quit", (10, 30), 
                        cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 255), 2)
            cv2.imshow("License Plate Detection", frame)
            
            # Check for quit command
            if cv2.waitKey(1) & 0xFF == ord('q'):
                break
            
    except KeyboardInterrupt:
        logger.info("Script terminated by user")
    except Exception as e:
        logger.error(f"Unexpected error: {e}")
    finally:
        # Cleanup
        logger.info("Cleaning up resources...")
        processing_thread.stop()
        processing_thread.join(timeout=1)
        
        cap.release()
        cv2.destroyAllWindows()
        
        lora.set_mode(MODE.SLEEP)
        BOARD.teardown()
        logger.info("LoRa connection closed")


if __name__ == "__main__":
    main()