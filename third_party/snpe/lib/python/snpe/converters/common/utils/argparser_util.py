import argparse


class ArgParserWrapper(object):
    """
    Wrapper class for argument parsing
    """
    def __init__(self, parents=[], **kwargs):
        self.parser = argparse.ArgumentParser(**kwargs)
        self.required = self.parser.add_argument_group('required arguments')
        self.optional = self.parser.add_argument_group('optional arguments')
        self._extend_from_parents(parents)

    def _extend_from_parents(self, parents):
        for i, parent in enumerate(parents):
            if not isinstance(parent, ArgParserWrapper):
                raise TypeError("Parent {0} not of Type ArgParser".format(parent.__class__.__name__))
            for action in parent.required._group_actions:
                self.required._add_action(action)
            for action in parent.optional._group_actions:
                self.optional._add_action(action)

    def add_required_argument(self, *args, **kwargs):
        self.required.add_argument(*args, required=True, **kwargs)

    def add_optional_argument(self, *args, **kwargs):
        self.optional.add_argument(*args, required=False, **kwargs)

    def parse_args(self, args=None, namespace=None):
        return self.parser.parse_args(args, namespace)
