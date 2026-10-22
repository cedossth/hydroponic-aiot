import os
import cv2
from ultralytics import YOLO
from PIL import Image
import requests
import io
from datetime import datetime
import subprocess
import numpy as np
import smtplib
from email.message import EmailMessage
import ssl

# Configuration de email
SMTP_SERVER = os.environ.get("SMTP_SERVER", "smtp.gmail.com")
SMTP_PORT = 587
GMAIL_USER = os.environ["ALERT_SENDER_EMAIL"]
GMAIL_PASS = os.environ["ALERT_SENDER_APP_PASSWORD"]


# Configuration CouchDB
COUCHDB_URL = os.environ.get("COUCHDB_URL", "http://localhost:5984")
COUCHDB_DB = 'plant_base'
COUCHDB_USER = os.environ["COUCHDB_USER"]
COUCHDB_PASS = os.environ["COUCHDB_PASSWORD"]

# Path to your TFLite model
model = YOLO("/home/hydrosystem/myenv/best_float32_disease_detect.tflite")


def capture_image():
    # Utiliser libcamera-jpeg pour capturer une image
    env = {"QT_QPA_PLATFORM": "offscreen"}
    subprocess.run(
        ["libcamera-jpeg", "--output", "test.jpg", "--width", "640", "--height", "480"],
        check=True,
        env=env
    )
    return cv2.imread("test.jpg")

def run_inference(image):
    if not isinstance(image, np.ndarray):
        raise TypeError("No valid picture")
    # Load the TFLite model and perform inference
    results = model(image)
    
    annotated_image = image.copy()
    detected_instances = 0
    
    if not hasattr(results[0], 'boxes') or len(results[0].boxes) == 0:
        print("Aucune boîte dtectée")
        return annotated_image, detected_instances
    
    for box in results[0].boxes:
        x1, y1, x2, y2 = map(int, box.xyxy[0])
        confidence = float(box.conf[0])
        label = box.cls[0]
        detected_instances += 1
        
        cv2.rectangle(annotated_image, (x1, y1), (x2, y2), (0, 255, 0), 2)
        cv2.putText(
            annotated_image,
            f"{label} {confidence:.2f}",
            (x1, y1 - 10),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.5,
            (0, 255, 0),
            2,
            )
    return annotated_image, detected_instances

def save_to_couchdb(detected_instances, annotated_image):
    # Save the image and results to CouchDB
    _, image_encoded = cv2.imencode('.jpg', annotated_image)
    image_data = image_encoded.tobytes()
    
    doc = {
        "timestamp": datetime.now().isoformat(),
        "detected_instances": detected_instances,
    }

    response = requests.post(
        f"{COUCHDB_URL}/{COUCHDB_DB}",
        json=doc,
        auth=(COUCHDB_USER, COUCHDB_PASS)
    )
    
    if response.status_code == 201:
        doc_id = response.json()["id"]
        doc_rev = response.json()["rev"]
        
        attachment_url = f"{COUCHDB_URL}/{COUCHDB_DB}/{doc_id}/image.jpg"
        headers = {"Content-Type": "image/jpeg"}
        attachment_response = requests.put(
            attachment_url,
            data=image_data,
            headers=headers,
            auth=(COUCHDB_USER, COUCHDB_PASS),
            params={"rev": doc_rev},
            )
        
        if attachment_response.status_code == 201:
            print("Image and results saved to CouchDB successfully with attachment.")
        else:
            print("Failed to save attachment to CouchDB:", attachment_response.content)
    else:
        print("Failed to save to CouchDB:", response.content)
        
if __name__ == "__main__":

    # Capture image
    images = capture_image()

    # Run the model
    annot_image, detect_instances = run_inference(images)

    # Save to CouchDB
    save_to_couchdb(detect_instances, annot_image)
    if detect_instances > 0:
        _, image_encoded = cv2.imencode('.jpg', annot_image)
        image_data = image_encoded.tobytes()
        
        msg = EmailMessage()
        msg["From"] = GMAIL_USER
        msg["To"] = os.environ["ALERT_RECIPIENT_EMAIL"]
        msg["Subject"] = "Alert object detect"
        msg.set_content(f"{detect_instances} plant diseases detects.")
        msg.add_attachment(image_data, maintype="image", subtype="jpeg", filename="annotated_image.jpg")
        
        try:
            context = ssl.create_default_context()
            with smtplib.SMTP(SMTP_SERVER, SMTP_PORT) as server:
                server.starttls(context=context)
                server.login(GMAIL_USER, GMAIL_PASS)
                server.send_message(msg)
            print("Email envoye")
        except Exception as e:
            print("Erreur lors de l'envoi de l'email :", e)