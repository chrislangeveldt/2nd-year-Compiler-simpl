================================================================================
                       SIGNING THE PLAGIARISM DECLARATION
================================================================================

The first time you have to sign the declaration:

1.  Create an image of your signature.  You can do this, for example, by signing
    a piece of paper, and taking a photograph with your webcam or cellphone.

2.  Crop this image so that there are no extra spaces on any side.

3.  Save this image as a JPEG or PNG file.

4.  Upload the image to NARGA, but DO NOT copy it into your repository.

Every time you have to sign:

1.  Change to the "subs" directory in your project repo.

2.  Run the following on the command line:
    $ ./sign.sh tag /path/to/signature_file

    where "tag" is the Git tag prescribed for the submission, and
    "/path/to/signature_file" is the path to your signature file.

	For example, if you are submitting the scanner, and your signature file is
	saved as "my_signature.png" in your home directory, you must do the
	following:

    $ ./sign.sh scanner ~/my_signature.png

3.  Remember to complete the submission report before doing your final commit
    for the deadline.
