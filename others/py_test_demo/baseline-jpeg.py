from PIL import Image

img = Image.open('C:\\Users\\25952\\Pictures\\jpg\\WCH.jpg')
img = img.convert('RGB')  # 确保是 RGB 格式
img.save('C:\\Users\\25952\\Pictures\\jpg\\WCH_baseline.jpg', 'JPEG', quality=95, progressive=False)  # progressive=False 即 baseline
img = Image.open('C:\\Users\\25952\\Pictures\\jpg\\WCH_baseline.jpg')
print(img.info)