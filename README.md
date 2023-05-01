## Boilerplate code for p3 (TEE) facial detection

### Changes (helpful if you want to hack SOD)
The following changes are added to make the CNN work.
- Model architecture `zfaceCNN`. The model hyperparameter defaults to a batch size of 128, far exceeding the TA memory limit. A new batch size of 1 is hard coded to every layers (convolutional, maxpool, and region) of the model. The changed functions are `parse_type` (`type in {convolutional, maxpool, region}`) in [sod.c](ta/sod.c) where `batch=param.batch` is substituted with `batch=1`. The width and height of CNN input is decreased from `416` to `96`.
- `load_weights`. The `load_weights` function takes a file stream to the saved CNN parameters. No filesystem is available in TEE. All `fread` is substituted with `fread_` that disguises a string as a file. The operation will `memcpy` string content to the destination, and move the string pointer forward for the number of bytes being read.
- Other support functions. `atoi` and `atof` are copied from [minlibc](ttps://github.com/GaloisInc/minlibc) to parse model architecture. `srand` is defined as a stub to prevent compile errors.

### Preprocessing
To encrypt images. Assuming all images are under imgs/input: 
```
python3 enc_dec.py -enc -i imgs/input -o imgs/input
```
Will generate imgs/input/*.aes

### Postprocessing
To decrypt images. Assuming all encrypted images are under imgs/output, named as *.aes
```
python3 enc_dec.py -dec -i imgs/output -o imgs/output
```

### Dataset
The two compressed files, `img_plain.tar` and `img_cipher.tar` contain test image files. The images come from [CelebA](https://mmlab.ie.cuhk.edu.hk/projects/CelebA.html) dataset (Liu, Ziwei, et al. "Deep learning face attributes in the wild." Proceedings of the IEEE international conference on computer vision. 2015.). As the names suggest, the former contain plaintext images, and the latter contain encrypted images. The key and nonce used are the same as the default ones in `enc_dec.py`. Move the two files to the shared folder with QEMU, and uncompress them with `tar -xvf *.tar`. Two new folders named `img_plain` and `img_cipher` will be extracted and have the face images for use.

### Compiling CA/TA
Follow build instructions from the README at the root of project folder.

### Running
After compiling, run the `optee_example_cnn` program under the folder that has the test images. The program accepts two arguments. They can be of the following three formats. The first one is given and serves as a reference while the next two need your implementation.

- Load CNN parameters and image, create CNN and detect face for every jpg and png files under the specified directory. The output files are saved under the same directory as input files. Their names are "out_" + original names. The plaintext model file [face_cnn.sod](./face_cnn.sod) is provided.
    ```
    optee_example_cnn face_cnn.sod img_plain/
    ```
    The CNN parameter file and image file should NOT be encrypted, i.e., they are just plaintext parameters and images. This interface is designed to easily debug sod.
- **TODO**: Load CNN parameters to TA and save it to a persistent storage project. A instance of `sod_cnn` named `pNet` will be created and can be used throughout the lifetime of the current TEE context. The encrypted model file [face_cnn.aes](./face_cnn.aes) is provided.
    ```
    optee_example_cnn -p face_cnn.aes
    ```
    The parameter file should be encrypted with the key and nonce in [ta/cnn_ta.c](ta/cnn_ta.c). 
- **TODO**: Load images under the specified directory to TA and detect faces in them. The output files are saved under the same directory as input files. Their names are "out_" + original names.
    ```
    optee_example_cnn -i img_cipher/
    ```
    The image file should be encrypted in the same way above. The saved result is encrypted with the same key and nonce.

### Known problems
Explore the limitations of this model by yourself!