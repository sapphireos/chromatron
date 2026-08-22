

class SyntaxError(Exception):
    def __init__(self, message='', lineno=None):
        self.lineno = lineno

        message += ' (line %d)' % (self.lineno)

        super().__init__(message)

class DivByZero(SyntaxError):
    pass

class CompilerFatal(Exception):
    def __init__(self, message=''):
        super(CompilerFatal, self).__init__(message)
