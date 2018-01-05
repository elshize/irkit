"""Retrieves statistics of WAND algorithm."""

import argparse
import json
import sys
import requests


def getparser():
    """Gets program's parser."""
    parser = argparse.ArgumentParser(
        prog='run', description='Get results (docs and scores).')

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
        print('query', 'rank', 'doc', 'score', sep=',')
    for idx, query in enumerate(args.infile):
        query = query[:-1]
        if '\t' in query:
            _, query = query.split('\t')
        request_data = {
            'type': 'taat',
            'k': args.k,
            'query': query
        }
        response = requests.post(args.url, json=request_data)
        resp_json = json.loads(response.text)
        for rank, result in enumerate(resp_json['results']):
            print(idx,
                  rank,
                  result['doc'],
                  result['score'],
                  sep=',')


if __name__ == '__main__':
    main()
