
class PixelArray():
    """An array of :py:class:`Pixel`

    """

    @property
    def hue(self):
        """Set the hue of all pixels in the array."""
        pass 

    @property
    def sat(self):
        """Set the saturation of all pixels in the array."""
        pass 

    @property
    def val(self):
        """Set the value of all pixels in the array."""
        pass 

    @property
    def hs_fade(self):
        """Set the hue/sat fader of all pixels in the array."""
        pass 

    @property
    def v_fade(self):
        """Set the value fader of all pixels in the array."""
        pass 

    @property
    def count(self):
        """Get the number of pixels in the array."""
        pass 

    @property
    def size_x(self):
        """Get the number of pixels in the X dimension of the array."""
        pass 

    @property
    def size_y(self):
        """Get the number of pixels in the Y dimension of the array."""
        pass 
    
    @property
    def is_fading(self):
        """Check if any pixels in the array are fading.

        :returns: 0 - no pixels are fading

                  1 - one or more pixels are fading

        """
        pass 


class Pixel():
    """A single pixel

    """

    @property
    def hue(self):
        """Get/set the hue of pixel."""
        pass 

    @property
    def sat(self):
        """Get/set the saturation of pixel."""
        pass 

    @property
    def val(self):
        """Get/set the value of pixel."""
        pass 

    @property
    def hs_fade(self):
        """Get/set the hue/sat fader of pixel."""
        pass 

    @property
    def v_fade(self):
        """Get/set the value fader of pixel."""
        pass 

    @property
    def is_fading(self):
        """Check if pixel is fading.

        :returns: 0 - pixel is not fading

                  1 - pixel is fading

        """
        pass 
