from PIL import Image, ImageDraw, ImageFont
import os

def create_icon(size, text, output_path):
    # Create a new image with white background
    image = Image.new('RGB', (size, size), 'white')
    draw = ImageDraw.Draw(image)
    
    # Draw a simple colored rectangle as background
    draw.rectangle([0, 0, size, size], fill='#0078d4')  # Microsoft blue color
    
    try:
        # Try to load a system font (size will be adjusted based on icon size)
        font_size = size // 4
        font = ImageFont.truetype("arial.ttf", font_size)
    except:
        # Fallback to default font if arial is not available
        font = ImageFont.load_default()
    
    # Add text to the center
    text = str(size)  # Using the size as text for easy identification
    bbox = draw.textbbox((0, 0), text, font=font)
    text_width = bbox[2] - bbox[0]
    text_height = bbox[3] - bbox[1]
    
    x = (size - text_width) // 2
    y = (size - text_height) // 2
    
    # Draw text in white
    draw.text((x, y), text, fill='white', font=font)
    
    # Ensure the Assets directory exists
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    
    # Save the image
    image.save(output_path, 'PNG')
    print(f"Created {output_path}")

def main():
    # Define the icons we need to generate
    icons = [
        (50, "PackageFiles/Assets/StoreLogo.png"),
        (44, "PackageFiles/Assets/Square44x44Logo.png"),
        (150, "PackageFiles/Assets/Square150x150Logo.png")
    ]
    
    # Generate each icon
    for size, path in icons:
        create_icon(size, str(size), path)

if __name__ == "__main__":
    main() 