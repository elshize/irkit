"""Retrieves statistics of WAND algorithm."""

import argparse
import json
import sys
import requests


def getparser():
    """Gets program's parser."""
    parser = argparse.ArgumentParser(
        prog='run', description='TA algorithm statistics.')

    parser.add_argument('url', help='The URL of the server.')
    parser.add_argument('infile', nargs='?', type=argparse.FileType('r'),
                        default=sys.stdin, help='Input file (optional).')
    parser.add_argument('-k', nargs='?', type=int, default=30)
    parser.add_argument('--skip-header', action='store_true', default=False)
    return parser


def main():
    """Main function."""
    parser = getparser()
    args = parser.parse_args()
    if not args.skip_header:
        print('terms', 'postings', 'processed', sep=',')
    for query in args.infile:
        query = query[:-1]
        if '\t' in query:
            _, query = query.split('\t')
        request_data = {
            'type': 'wand',
            'k': args.k,
            'query': query
        }
        response = requests.post(args.url, json=request_data)
        resp_json = json.loads(response.text)
        print(len(query.split(' ')),
              resp_json['stats']['postings'],
              resp_json['stats']['processed'],
              sep=',')


if __name__ == '__main__':
    main()
